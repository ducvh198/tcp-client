#ifndef MODE_INTERACTIVE_H
#define MODE_INTERACTIVE_H

#include "cli_args.h"

/**
 * Runs interactive terminal mode using POSIX poll() multiplexing.
 *
 * Multiplexes STDIN_FILENO and sockfd.
 * Displays prompt string "> ", sends typed input lines to server,
 * handles exit/quit commands (case-insensitive), Ctrl+D/Ctrl+C, and
 * streams real-time server responses to STDOUT.
 *
 * @param sockfd Connected socket file descriptor.
 * @param config Pointer to CLI configuration options.
 * @return 0 on normal quit ("exit", "quit", Ctrl+D/Ctrl+C, clean disconnect),
 *         5 on network socket I/O error.
 */
int run_interactive_mode(int sockfd, const cli_config_t *config);

#endif /* MODE_INTERACTIVE_H */
