#ifndef MODE_ONESHOT_H
#define MODE_ONESHOT_H

#include "cli_args.h"

/**
 * Runs one-shot / pipe mode.
 *
 * Reads payload from STDIN_FILENO in binary chunks, transmits to TCP socket via
 * socket_write_all(), issues shutdown(sockfd, SHUT_WR) TCP half-close,
 * streams server response to STDOUT_FILENO, and flushes output.
 *
 * @param sockfd Connected TCP socket file descriptor.
 * @param config Pointer to parsed CLI configuration.
 * @return 0 on success, 4 on timeout error, 5 on network/socket I/O error.
 */
int run_oneshot_mode(int sockfd, const cli_config_t *config);

#endif /* MODE_ONESHOT_H */
