#include "mode_interactive.h"
#include "socket_client.h"
#include "signal_handler.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #ifndef strncasecmp
  #define strncasecmp _strnicmp
  #endif
#else
  #include <strings.h>
#endif

#define INTERACTIVE_BUF_SIZE 65536

static bool is_exit_command(const char *buf) {
    if (!buf) {
        return false;
    }

    /* Trim leading whitespace */
    const char *start = buf;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (strncasecmp(start, "exit", 4) == 0) {
        const char *p = start + 4;
        while (*p && (*p == '\n' || *p == '\r' || isspace((unsigned char)*p))) p++;
        if (*p == '\0') return true;
    }
    if (strncasecmp(start, "quit", 4) == 0) {
        const char *p = start + 4;
        while (*p && (*p == '\n' || *p == '\r' || isspace((unsigned char)*p))) p++;
        if (*p == '\0') return true;
    }

    return false;
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

int run_interactive_mode(int sockfd, const cli_config_t *config) {
    if (sockfd < 0 || !config) {
        return 5;
    }

    if (config->verbose) {
        fprintf(stderr, "[VERBOSE] Entering Interactive Mode. Type 'exit' or 'quit' to disconnect.\n");
    }

    char sock_buf[INTERACTIVE_BUF_SIZE];
    char raw_stdin[INTERACTIVE_BUF_SIZE];

    char *line_buf = NULL;
    size_t line_cap = 0;
    size_t line_len = 0;

    bool exit_requested = false;
    bool stdin_eof = false;

    printf("> ");
    fflush(stdout);

    while (!signal_handler_is_interrupted()) {
#ifndef _WIN32
        pollfd_t fds[2];
        int nfds = 0;

        int stdin_idx = -1;
        int sock_idx = -1;

        if (!stdin_eof) {
            stdin_idx = nfds;
            fds[nfds].fd = sockfd;
            fds[nfds].fd = STDIN_FILENO;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        sock_idx = nfds;
        fds[nfds].fd = sockfd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;

        int poll_timeout = stdin_eof ? 200 : 100;

        int poll_rc = poll_sockets(fds, nfds, poll_timeout);
        if (poll_rc < 0) {
            if (get_last_socket_error() == EINTR) {
                if (signal_handler_is_interrupted()) {
                    break;
                }
                continue;
            }
            fprintf(stderr, "Error: poll() system call failed\n");
            free(line_buf);
            return 5;
        }

        if (poll_rc == 0 && stdin_eof) {
            free(line_buf);
            return 0;
        }

        /* Check socket event */
        if (sock_idx >= 0 && (fds[sock_idx].revents & POLLIN)) {
            ssize_t nread = socket_read(sockfd, sock_buf, sizeof(sock_buf), config->timeout_ms);
            if (nread > 0) {
                fwrite(sock_buf, 1, (size_t)nread, stdout);
                fflush(stdout);
                if (!stdin_eof) {
                    printf("> ");
                    fflush(stdout);
                }
            } else if (nread == 0) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] Server closed connection.\n");
                }
                free(line_buf);
                return 0;
            } else {
                if (nread != SOCKET_ERR_TIMEOUT) {
                    fprintf(stderr, "Error: Socket read failed or connection lost.\n");
                    free(line_buf);
                    return 5;
                }
            }
        } else if (sock_idx >= 0 && (fds[sock_idx].revents & (POLLHUP | POLLERR))) {
            if (config->verbose) {
                fprintf(stderr, "[VERBOSE] Server hangup / error event detected.\n");
            }
            free(line_buf);
            return 0;
        }

        /* Check STDIN event */
        if (stdin_idx >= 0 && (fds[stdin_idx].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t nread = read(STDIN_FILENO, raw_stdin, sizeof(raw_stdin));
            if (nread == 0) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] STDIN EOF detected.\n");
                }
                stdin_eof = true;
                if (line_len > 0) {
                    if (line_len + 1 > line_cap) {
                        size_t new_cap = line_len + 1;
                        char *nb = realloc(line_buf, new_cap);
                        if (nb) { line_buf = nb; line_cap = new_cap; }
                    }
                    if (line_buf) {
                        line_buf[line_len] = '\0';
                        if (is_exit_command(line_buf)) {
                            exit_requested = true;
                        } else {
                            socket_write_all(sockfd, line_buf, line_len, config->timeout_ms);
                            line_len = 0;
                        }
                    }
                }
                if (exit_requested) {
                    break;
                }
            } else if (nread < 0) {
                if (!is_socket_wouldblock(get_last_socket_error())) {
                    stdin_eof = true;
                }
            } else {
                for (ssize_t i = 0; i < nread; i++) {
                    char c = raw_stdin[i];

                    if (line_len + 2 > line_cap) {
                        size_t new_cap = (line_cap == 0) ? 4096 : line_cap * 2;
                        if (new_cap < line_len + 2) {
                            new_cap = line_len + 2;
                        }
                        char *new_buf = realloc(line_buf, new_cap);
                        if (!new_buf) {
                            fprintf(stderr, "Error: Memory allocation failed for input line buffer.\n");
                            free(line_buf);
                            return 5;
                        }
                        line_buf = new_buf;
                        line_cap = new_cap;
                    }

                    line_buf[line_len++] = c;

                    if (c == '\n') {
                        line_buf[line_len] = '\0';
                        if (is_exit_command(line_buf)) {
                            socket_write_all(sockfd, line_buf, line_len, config->timeout_ms);
                            line_len = 0;
                            exit_requested = true;
                            break;
                        }

                        ssize_t nsent = socket_write_all(sockfd, line_buf, line_len, config->timeout_ms);
                        if (nsent < 0) {
                            fprintf(stderr, "Error: Failed to transmit data to server.\n");
                            free(line_buf);
                            return 5;
                        }
                        line_len = 0;
                    }
                }
            }
        }
#else
        /* Windows loop using WSAPoll for socket and check_stdin_ready() for STDIN */
        pollfd_t spfd;
        spfd.fd = sockfd;
        spfd.events = POLLIN;
        spfd.revents = 0;

        int poll_rc = poll_sockets(&spfd, 1, 50);
        if (poll_rc > 0 && (spfd.revents & POLLIN)) {
            ssize_t nread = socket_read(sockfd, sock_buf, sizeof(sock_buf), config->timeout_ms);
            if (nread > 0) {
                fwrite(sock_buf, 1, (size_t)nread, stdout);
                fflush(stdout);
                if (!stdin_eof) {
                    printf("> ");
                    fflush(stdout);
                }
            } else if (nread == 0) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] Server closed connection.\n");
                }
                free(line_buf);
                return 0;
            }
        }

        if (!stdin_eof && check_stdin_ready()) {
            int nread = _read(STDIN_FILENO, raw_stdin, sizeof(raw_stdin));
            if (nread == 0) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] STDIN EOF detected.\n");
                }
                stdin_eof = true;
                if (exit_requested) break;
            } else if (nread > 0) {
                for (int i = 0; i < nread; i++) {
                    char c = raw_stdin[i];
                    if (line_len + 2 > line_cap) {
                        size_t new_cap = (line_cap == 0) ? 4096 : line_cap * 2;
                        char *new_buf = realloc(line_buf, new_cap);
                        if (!new_buf) {
                            free(line_buf);
                            return 5;
                        }
                        line_buf = new_buf;
                        line_cap = new_cap;
                    }
                    line_buf[line_len++] = c;

                    if (c == '\n') {
                        line_buf[line_len] = '\0';
                        if (is_exit_command(line_buf)) {
                            socket_write_all(sockfd, line_buf, line_len, config->timeout_ms);
                            line_len = 0;
                            exit_requested = true;
                            break;
                        }
                        ssize_t nsent = socket_write_all(sockfd, line_buf, line_len, config->timeout_ms);
                        if (nsent < 0) {
                            free(line_buf);
                            return 5;
                        }
                        line_len = 0;
                    }
                }
            }
        }
#endif

        if (exit_requested) {
            pollfd_t spfd;
            spfd.fd = sockfd;
            spfd.events = POLLIN;
            spfd.revents = 0;
            while (poll_sockets(&spfd, 1, 300) > 0 && (spfd.revents & POLLIN)) {
                ssize_t nread = socket_read(sockfd, sock_buf, sizeof(sock_buf), config->timeout_ms);
                if (nread > 0) {
                    fwrite(sock_buf, 1, (size_t)nread, stdout);
                    fflush(stdout);
                } else {
                    break;
                }
            }
            if (config->verbose) {
                fprintf(stderr, "[VERBOSE] User requested exit.\n");
            }
            free(line_buf);
            return 0;
        }
    }

    free(line_buf);
    return 0;
}
