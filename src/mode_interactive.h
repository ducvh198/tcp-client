#ifndef MODE_INTERACTIVE_H
#define MODE_INTERACTIVE_H

#include "cli_args.h"
#include "linenoise.h"

/**
 * Tab completion callback for Linenoise interactive terminal.
 * Completes built-in control commands and sample HSM/payment commands.
 *
 * @param buf Current input buffer string being typed.
 * @param lc Pointer to linenoiseCompletions struct where matches are added.
 */
void interactive_completion_callback(const char *buf, linenoiseCompletions *lc);

/**
 * Resolves the path to the persistent interactive history file.
 *
 * Windows: %USERPROFILE%\.tcp_client_history or %HOMEDRIVE%%HOMEPATH%\.tcp_client_history
 * POSIX:   $HOME/.tcp_client_history
 * Fallback: ./.tcp_client_history
 *
 * @param out_path Destination buffer to receive the resolved path.
 * @param max_len Maximum length of out_path buffer.
 * @return out_path pointer (or NULL if out_path is NULL or max_len is 0).
 */
char *get_interactive_history_path(char *out_path, size_t max_len);

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
