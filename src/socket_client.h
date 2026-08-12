#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>

#define SOCKET_SUCCESS          0
#define SOCKET_ERR_DNS         -2  /* Host resolution / DNS failure */
#define SOCKET_ERR_REFUSED     -3  /* Connection refused / Host unreachable */
#define SOCKET_ERR_TIMEOUT     -4  /* Connection or read/write timeout */
#define SOCKET_ERR_IO          -5  /* Network socket I/O error / Disconnect */

/**
 * Establishes a non-blocking TCP socket connection to a host and port.
 *
 * Supports IPv4 and IPv6 dual-stack resolution via getaddrinfo.
 * Enforces connection timeout using non-blocking socket + poll() + SO_ERROR verification.
 *
 * @param host       Hostname or IP address string.
 * @param port       Port number (1-65535).
 * @param timeout_ms Connection timeout in milliseconds (>0).
 * @param verbose    If true, outputs connection diagnostics to stderr.
 * @return Non-negative socket file descriptor on success, or negative error code:
 *         SOCKET_ERR_DNS (-2), SOCKET_ERR_REFUSED (-3), SOCKET_ERR_TIMEOUT (-4), SOCKET_ERR_IO (-5).
 */
int socket_connect(const char *host, int port, int timeout_ms, bool verbose);

/**
 * Transmits all data in buffer to socket with timeout enforcement.
 *
 * Uses MSG_NOSIGNAL to suppress SIGPIPE on broken connections.
 *
 * @param sockfd     Connected socket descriptor.
 * @param buf        Pointer to output data buffer.
 * @param len        Number of bytes to write.
 * @param timeout_ms Timeout in milliseconds.
 * @return Total bytes written on success, or SOCKET_ERR_TIMEOUT (-4) / SOCKET_ERR_IO (-5).
 */
ssize_t socket_write_all(int sockfd, const void *buf, size_t len, int timeout_ms);

/**
 * Reads up to len bytes from socket with timeout enforcement.
 *
 * @param sockfd     Connected socket descriptor.
 * @param buf        Pointer to receive buffer.
 * @param len        Buffer capacity.
 * @param timeout_ms Timeout in milliseconds.
 * @return Number of bytes read (>0), 0 on clean server EOF, or SOCKET_ERR_TIMEOUT (-4) / SOCKET_ERR_IO (-5).
 */
ssize_t socket_read(int sockfd, void *buf, size_t len, int timeout_ms);

/**
 * Safely closes an open socket file descriptor.
 *
 * @param sockfd Socket file descriptor.
 */
void socket_close(int sockfd);

#endif /* SOCKET_CLIENT_H */
