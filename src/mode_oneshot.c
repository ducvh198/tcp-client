#include "mode_oneshot.h"
#include "socket_client.h"
#include "signal_handler.h"
#include "hex_utils.h"
#include "hsm_decoder.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ONESHOT_BUF_SIZE 65536

static ssize_t write_all_fd(int fd, const void *buf, size_t count) {
    const char *ptr = (const char *)buf;
    size_t left = count;
    while (left > 0) {
#ifdef _WIN32
        int w = _write(fd, ptr, (unsigned int)left);
#else
        ssize_t w = write(fd, ptr, left);
#endif
        if (w > 0) {
            ptr += w;
            left -= (size_t)w;
        } else if (w < 0) {
            if (get_last_socket_error() == EINTR) continue;
            return -1;
        } else {
            return -1;
        }
    }
    return (ssize_t)count;
}

#ifdef _WIN32
static bool check_stdin_ready(void) {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) return false;
    DWORD mode;
    if (GetConsoleMode(hStdin, &mode)) {
        INPUT_RECORD ir[1];
        DWORD numRead = 0;
        if (PeekConsoleInput(hStdin, ir, 1, &numRead) && numRead > 0) {
            return (ir[0].EventType == KEY_EVENT && ir[0].Event.KeyEvent.bKeyDown);
        }
        return false;
    } else {
        DWORD avail = 0;
        if (PeekNamedPipe(hStdin, NULL, 0, NULL, &avail, NULL)) {
            return avail > 0;
        }
        return true;
    }
}
#endif

int run_oneshot_mode(int sockfd, const cli_config_t *config) {
    if (sockfd < 0 || !config) {
        return 5;
    }

    int timeout_ms = config->timeout_ms;
    if (timeout_ms <= 0) {
        timeout_ms = 5000;
    }

    if (config->verbose) {
        fprintf(stderr, "[VERBOSE] Entering One-Shot Mode.\n");
    }

    set_socket_nonblocking(sockfd);

    char send_buf[ONESHOT_BUF_SIZE];
    size_t send_buf_len = 0;
    size_t send_buf_pos = 0;
    bool stdin_eof = false;
    bool shutdown_done = false;

    if (config->is_hex) {
        uint8_t hex_bytes[ONESHOT_BUF_SIZE];
        size_t hex_len = 0;
        int hex_rc = hex_to_bytes(config->hex_payload, hex_bytes, sizeof(hex_bytes), &hex_len);
        if (hex_rc != 0) {
            fprintf(stderr, "Error: Invalid HEX string format '%s'. Ensure even length and valid hex characters.\n", config->hex_payload);
            return 1;
        }
        if (config->add_tcp_len) {
            send_buf[0] = (char)((hex_len >> 8) & 0xFF);
            send_buf[1] = (char)(hex_len & 0xFF);
            memcpy(send_buf + 2, hex_bytes, hex_len);
            send_buf_len = hex_len + 2;
        } else {
            memcpy(send_buf, hex_bytes, hex_len);
            send_buf_len = hex_len;
        }
        send_buf_pos = 0;
        stdin_eof = true;
        if (config->verbose) {
            fprintf(stderr, "[VERBOSE] Transmitting %lu bytes of HEX payload (TCP len header: %s).\n",
                    (unsigned long)send_buf_len, config->add_tcp_len ? "yes" : "no");
        }
    } else if (config->is_ascii) {
        size_t ascii_len = strlen(config->ascii_payload);
        if (config->add_tcp_len) {
            send_buf[0] = (char)((ascii_len >> 8) & 0xFF);
            send_buf[1] = (char)(ascii_len & 0xFF);
            memcpy(send_buf + 2, config->ascii_payload, ascii_len);
            send_buf_len = ascii_len + 2;
        } else {
            memcpy(send_buf, config->ascii_payload, ascii_len);
            send_buf_len = ascii_len;
        }
        send_buf_pos = 0;
        stdin_eof = true;
        if (config->verbose) {
            fprintf(stderr, "[VERBOSE] Transmitting %lu bytes of ASCII payload (TCP len header: %s).\n",
                    (unsigned long)send_buf_len, config->add_tcp_len ? "yes" : "no");
        }
    }

    char recv_buf[ONESHOT_BUF_SIZE];
    bool socket_eof = false;

    uint8_t hsm_accum_buf[ONESHOT_BUF_SIZE];
    size_t hsm_accum_len = 0;

    while (!signal_handler_is_interrupted() && !socket_eof) {
        if (stdin_eof && send_buf_len == 0 && !shutdown_done) {
            if (config->verbose) {
                fprintf(stderr, "[VERBOSE] STDIN EOF reached. Issuing shutdown(SHUT_WR)...\n");
            }
            shutdown(sockfd, SHUT_WR);
            shutdown_done = true;
        }

#ifndef _WIN32
        pollfd_t fds[2];
        int nfds = 0;

        int stdin_idx = -1;
        int sock_idx = -1;

        if (!stdin_eof && send_buf_len == 0) {
            stdin_idx = nfds;
            fds[nfds].fd = STDIN_FILENO;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        if (send_buf_len > 0 || !socket_eof) {
            sock_idx = nfds;
            fds[nfds].fd = sockfd;
            fds[nfds].events = 0;
            if (send_buf_len > 0) {
                fds[nfds].events |= POLLOUT;
            }
            if (!socket_eof) {
                fds[nfds].events |= POLLIN;
            }
            fds[nfds].revents = 0;
            nfds++;
        }

        if (nfds == 0) {
            break;
        }

        int poll_rc = poll_sockets(fds, nfds, timeout_ms);
        if (poll_rc < 0) {
            if (get_last_socket_error() == EINTR) {
                continue;
            }
            return 5;
        } else if (poll_rc == 0) {
            fprintf(stderr, "Error: Read timeout waiting for server response.\n");
            return 4;
        }

        /* Socket I/O */
        if (sock_idx >= 0 && (fds[sock_idx].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t nread = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
            if (nread > 0) {
                if (config->decode_hsm) {
                    if (hsm_accum_len + (size_t)nread < sizeof(hsm_accum_buf)) {
                        memcpy(hsm_accum_buf + hsm_accum_len, recv_buf, (size_t)nread);
                        hsm_accum_len += (size_t)nread;
                        if (hsm_accum_len >= 2) {
                            uint16_t expected_payload = (uint16_t)(((uint8_t)hsm_accum_buf[0] << 8) | (uint8_t)hsm_accum_buf[1]);
                            size_t expected_total = 2 + (size_t)expected_payload;
                            if (hsm_accum_len >= expected_total) {
                                socket_eof = true;
                            }
                        }
                    }
                } else if (config->hex_out) {
                    char hex_out_buf[ONESHOT_BUF_SIZE * 3 + 2];
                    if (bytes_to_hex((const uint8_t *)recv_buf, (size_t)nread, hex_out_buf, sizeof(hex_out_buf), true, true) == 0) {
                        size_t hex_len = strlen(hex_out_buf);
                        hex_out_buf[hex_len] = '\n';
                        hex_out_buf[hex_len + 1] = '\0';
                        if (write_all_fd(STDOUT_FILENO, hex_out_buf, hex_len + 1) < 0) {
                            return 5;
                        }
                    }
                } else {
                    if (write_all_fd(STDOUT_FILENO, recv_buf, (size_t)nread) < 0) {
                        return 5;
                    }
                }
            } else if (nread == 0) {
                socket_eof = true;
            } else {
                if (!is_socket_wouldblock(get_last_socket_error())) {
                    return 5;
                }
            }
        }

        /* STDIN I/O */
        if (stdin_idx >= 0 && (fds[stdin_idx].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t nread = read(STDIN_FILENO, send_buf, sizeof(send_buf));
            if (nread > 0) {
                send_buf_len = (size_t)nread;
                send_buf_pos = 0;
            } else if (nread == 0) {
                stdin_eof = true;
            } else {
                if (!is_socket_wouldblock(get_last_socket_error())) {
                    return 5;
                }
            }
        }

        /* Socket Send */
        if (sock_idx >= 0 && (fds[sock_idx].revents & POLLOUT) && send_buf_len > 0) {
            ssize_t nsent = send(sockfd, send_buf + send_buf_pos, (int)(send_buf_len - send_buf_pos), MSG_NOSIGNAL);
            if (nsent > 0) {
                send_buf_pos += (size_t)nsent;
                if (send_buf_pos >= send_buf_len) {
                    send_buf_len = 0;
                    send_buf_pos = 0;
                }
            } else if (nsent < 0) {
                if (!is_socket_wouldblock(get_last_socket_error())) {
                    return 5;
                }
            }
        }
#else
        /* Windows loop */
        pollfd_t spfd;
        spfd.fd = sockfd;
        spfd.events = 0;
        if (send_buf_len > 0) spfd.events |= POLLOUT;
        if (!socket_eof) spfd.events |= POLLIN;
        spfd.revents = 0;

        int poll_rc = poll_sockets(&spfd, 1, 50);
        if (poll_rc < 0) {
            return 5;
        }

        if (spfd.revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t nread = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
            if (nread > 0) {
                if (config->decode_hsm) {
                    if (hsm_accum_len + (size_t)nread < sizeof(hsm_accum_buf)) {
                        memcpy(hsm_accum_buf + hsm_accum_len, recv_buf, (size_t)nread);
                        hsm_accum_len += (size_t)nread;
                        if (hsm_accum_len >= 2) {
                            uint16_t expected_payload = (uint16_t)(((uint8_t)hsm_accum_buf[0] << 8) | (uint8_t)hsm_accum_buf[1]);
                            size_t expected_total = 2 + (size_t)expected_payload;
                            if (hsm_accum_len >= expected_total) {
                                socket_eof = true;
                            }
                        }
                    }
                } else if (config->hex_out) {
                    char hex_out_buf[ONESHOT_BUF_SIZE * 3 + 2];
                    if (bytes_to_hex((const uint8_t *)recv_buf, (size_t)nread, hex_out_buf, sizeof(hex_out_buf), true, true) == 0) {
                        size_t hex_len = strlen(hex_out_buf);
                        hex_out_buf[hex_len] = '\n';
                        hex_out_buf[hex_len + 1] = '\0';
                        write_all_fd(STDOUT_FILENO, hex_out_buf, hex_len + 1);
                    }
                } else {
                    write_all_fd(STDOUT_FILENO, recv_buf, (size_t)nread);
                }
            } else if (nread == 0) {
                socket_eof = true;
            }
        }

        if ((spfd.revents & POLLOUT) && send_buf_len > 0) {
            ssize_t nsent = send(sockfd, send_buf + send_buf_pos, (int)(send_buf_len - send_buf_pos), MSG_NOSIGNAL);
            if (nsent > 0) {
                send_buf_pos += (size_t)nsent;
                if (send_buf_pos >= send_buf_len) {
                    send_buf_len = 0;
                    send_buf_pos = 0;
                }
            }
        }

        if (!stdin_eof && send_buf_len == 0 && check_stdin_ready()) {
            int nread = _read(STDIN_FILENO, send_buf, sizeof(send_buf));
            if (nread > 0) {
                send_buf_len = (size_t)nread;
                send_buf_pos = 0;
            } else if (nread == 0) {
                stdin_eof = true;
            }
        }
#endif
    }

    if (stdin_eof && send_buf_len == 0 && !shutdown_done) {
        shutdown(sockfd, SHUT_WR);
    }

    if (config->decode_hsm && hsm_accum_len > 0) {
        hsm_response_t hsm_resp;
        if (hsm_parse_response(hsm_accum_buf, hsm_accum_len, (size_t)config->hsm_header_len, &hsm_resp) == 0) {
            char analysis_buf[4096];
            hsm_format_analysis(&hsm_resp, hsm_accum_buf, hsm_accum_len, analysis_buf, sizeof(analysis_buf));
            write_all_fd(STDOUT_FILENO, analysis_buf, strlen(analysis_buf));
        } else {
            fprintf(stderr, "Error: Unable to parse HSM response packet structure.\n");
        }
    }

    return 0;
}
