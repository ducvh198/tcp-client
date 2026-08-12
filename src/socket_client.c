#include "socket_client.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int socket_connect(const char *host, int port, int timeout_ms, bool verbose) {
    if (!host || host[0] == '\0' || port < 1 || port > 65535) {
        return SOCKET_ERR_IO;
    }
    if (timeout_ms < 0) {
        timeout_ms = 5000;
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (verbose) {
        fprintf(stderr, "[VERBOSE] Resolving host '%s' port %d...\n", host, port);
    }

    int s = getaddrinfo(host, port_str, &hints, &res);
    if (s != 0) {
        if (verbose) {
            fprintf(stderr, "[VERBOSE] Host resolution failed for '%s'\n", host);
        }
        return SOCKET_ERR_DNS;
    }

    int last_error = SOCKET_ERR_REFUSED;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        int sockfd = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (set_socket_nonblocking(sockfd) < 0) {
            platform_socket_close(sockfd);
            last_error = SOCKET_ERR_IO;
            continue;
        }

        if (verbose) {
            fprintf(stderr, "[VERBOSE] Connecting to target address...\n");
        }

        int rc = connect(sockfd, rp->ai_addr, (socklen_t)rp->ai_addrlen);
        if (rc == 0) {
            /* Connected immediately */
            freeaddrinfo(res);
            if (verbose) {
                fprintf(stderr, "[VERBOSE] Connected immediately (sockfd: %d)\n", sockfd);
            }
            return sockfd;
        }

        int err = get_last_socket_error();
        if (rc < 0 && !is_socket_wouldblock(err)) {
            platform_socket_close(sockfd);
            if (is_socket_refused(err)) {
                last_error = SOCKET_ERR_REFUSED;
            } else {
                last_error = SOCKET_ERR_IO;
            }
            continue;
        }

        pollfd_t pfd;
        pfd.fd = sockfd;
        pfd.events = POLLOUT | POLLIN;
        pfd.revents = 0;

        int poll_rc = poll_sockets(&pfd, 1, timeout_ms);
        if (poll_rc == 0) {
            if (verbose) {
                fprintf(stderr, "[VERBOSE] Connection attempt timed out (%d ms)\n", timeout_ms);
            }
            platform_socket_close(sockfd);
            last_error = SOCKET_ERR_TIMEOUT;
            continue;
        } else if (poll_rc < 0) {
            platform_socket_close(sockfd);
            last_error = SOCKET_ERR_IO;
            continue;
        }

        int so_error = 0;
        socklen_t len = sizeof(so_error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len) < 0) {
            platform_socket_close(sockfd);
            last_error = SOCKET_ERR_IO;
            continue;
        }

        if (so_error == 0) {
            freeaddrinfo(res);
            if (verbose) {
                fprintf(stderr, "[VERBOSE] Connected successfully (sockfd: %d)\n", sockfd);
            }
            return sockfd;
        } else {
            platform_socket_close(sockfd);
            if (is_socket_refused(so_error)) {
                last_error = SOCKET_ERR_REFUSED;
            } else if (is_socket_timeout(so_error)) {
                last_error = SOCKET_ERR_TIMEOUT;
            } else {
                last_error = SOCKET_ERR_IO;
            }
            continue;
        }
    }

    freeaddrinfo(res);
    return last_error;
}

ssize_t socket_write_all(int sockfd, const void *buf, size_t len, int timeout_ms) {
    if (sockfd < 0 || !buf) {
        return SOCKET_ERR_IO;
    }
    const char *ptr = (const char *)buf;
    size_t remaining = len;

    while (remaining > 0) {
        pollfd_t pfd;
        pfd.fd = sockfd;
        pfd.events = POLLOUT;
        pfd.revents = 0;

        int poll_rc = poll_sockets(&pfd, 1, timeout_ms);
        if (poll_rc == 0) {
            return SOCKET_ERR_TIMEOUT;
        } else if (poll_rc < 0) {
            int err = get_last_socket_error();
            if (is_socket_wouldblock(err)) continue;
            return SOCKET_ERR_IO;
        }

        ssize_t nsent = send(sockfd, ptr, (int)remaining, MSG_NOSIGNAL);
        if (nsent > 0) {
            ptr += nsent;
            remaining -= (size_t)nsent;
        } else if (nsent == 0) {
            return SOCKET_ERR_IO;
        } else {
            int err = get_last_socket_error();
            if (is_socket_wouldblock(err)) continue;
            return SOCKET_ERR_IO;
        }
    }
    return (ssize_t)len;
}

ssize_t socket_read(int sockfd, void *buf, size_t len, int timeout_ms) {
    if (sockfd < 0 || !buf) {
        return SOCKET_ERR_IO;
    }
    pollfd_t pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int poll_rc = poll_sockets(&pfd, 1, timeout_ms);
    if (poll_rc == 0) {
        return SOCKET_ERR_TIMEOUT;
    } else if (poll_rc < 0) {
        int err = get_last_socket_error();
        if (is_socket_wouldblock(err)) return 0;
        return SOCKET_ERR_IO;
    }

    ssize_t nread = recv(sockfd, (char *)buf, (int)len, 0);
    if (nread > 0) {
        return nread;
    } else if (nread == 0) {
        return 0; /* Clean EOF */
    } else {
        int err = get_last_socket_error();
        if (is_socket_wouldblock(err)) {
            return SOCKET_ERR_TIMEOUT;
        }
        return SOCKET_ERR_IO;
    }
}

ssize_t socket_read_exact(int sockfd, void *buf, size_t len, int timeout_ms) {
    if (sockfd < 0 || !buf) {
        return SOCKET_ERR_IO;
    }
    char *ptr = (char *)buf;
    size_t accumulated = 0;

    while (accumulated < len) {
        ssize_t n = socket_read(sockfd, ptr + accumulated, len - accumulated, timeout_ms);
        if (n > 0) {
            accumulated += (size_t)n;
        } else if (n == 0) {
            return (ssize_t)accumulated; /* Clean EOF before full len */
        } else {
            return n; /* SOCKET_ERR_TIMEOUT or SOCKET_ERR_IO */
        }
    }
    return (ssize_t)accumulated;
}

ssize_t socket_read_framed(int sockfd, void *buf, size_t max_len, int timeout_ms) {
    if (sockfd < 0 || !buf || max_len < 2) {
        return SOCKET_ERR_IO;
    }
    uint8_t *u8_buf = (uint8_t *)buf;

    ssize_t header_rc = socket_read_exact(sockfd, u8_buf, 2, timeout_ms);
    if (header_rc <= 0) {
        return header_rc;
    }
    if (header_rc < 2) {
        return SOCKET_ERR_IO;
    }

    uint16_t payload_len = (uint16_t)((u8_buf[0] << 8) | u8_buf[1]);
    size_t total_frame_len = 2 + (size_t)payload_len;

    if (total_frame_len > max_len) {
        return SOCKET_ERR_IO;
    }

    if (payload_len > 0) {
        ssize_t payload_rc = socket_read_exact(sockfd, u8_buf + 2, payload_len, timeout_ms);
        if (payload_rc < 0) {
            return payload_rc;
        }
        if ((size_t)payload_rc < payload_len) {
            return SOCKET_ERR_IO;
        }
    }

    return (ssize_t)total_frame_len;
}

void socket_close(int sockfd) {
    if (sockfd >= 0) {
        platform_socket_close(sockfd);
    }
}
