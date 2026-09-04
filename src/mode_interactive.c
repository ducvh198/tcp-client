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
  #include <conio.h>
  #include <io.h>
#else
  #include <strings.h>
  #include <unistd.h>
#endif

#define INTERACTIVE_BUF_SIZE 65536

static const char * const INTERACTIVE_COMPLETIONS[] = {
    /* Built-in Control Commands */
    "exit",
    "quit",
    "help",
    "clear",
    "status",
    "history",
    /* Sample HSM & Payment Commands */
    "NC0000",
    "00 06 30 30 30 30",
    "BA",
    "BB",
    "CA",
    "CB",
    "M0",
    "M2"
};

#define INTERACTIVE_COMPLETIONS_COUNT \
    (sizeof(INTERACTIVE_COMPLETIONS) / sizeof(INTERACTIVE_COMPLETIONS[0]))

void interactive_completion_callback(const char *buf, linenoiseCompletions *lc) {
    if (!lc) {
        return;
    }

    const char *prefix = buf ? buf : "";
    size_t prefix_len = strlen(prefix);

    for (size_t i = 0; i < INTERACTIVE_COMPLETIONS_COUNT; i++) {
        const char *cmd = INTERACTIVE_COMPLETIONS[i];
        if (prefix_len == 0 || strncmp(cmd, prefix, prefix_len) == 0) {
            linenoiseAddCompletion(lc, cmd);
        }
    }
}

char *get_interactive_history_path(char *out_path, size_t max_len) {
    if (!out_path || max_len == 0) {
        return NULL;
    }

#ifdef _WIN32
    const char *userprofile = getenv("USERPROFILE");
    if (userprofile && userprofile[0] != '\0') {
        int n = snprintf(out_path, max_len, "%s\\.tcp_client_history", userprofile);
        if (n < 0 || (size_t)n >= max_len) {
            snprintf(out_path, max_len, "./.tcp_client_history");
        }
    } else {
        const char *homedrive = getenv("HOMEDRIVE");
        const char *homepath = getenv("HOMEPATH");
        if (homedrive && homedrive[0] != '\0' && homepath && homepath[0] != '\0') {
            int n = snprintf(out_path, max_len, "%s%s\\.tcp_client_history", homedrive, homepath);
            if (n < 0 || (size_t)n >= max_len) {
                snprintf(out_path, max_len, "./.tcp_client_history");
            }
        } else {
            snprintf(out_path, max_len, "./.tcp_client_history");
        }
    }
#else
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        int n = snprintf(out_path, max_len, "%s/.tcp_client_history", home);
        if (n < 0 || (size_t)n >= max_len) {
            snprintf(out_path, max_len, "./.tcp_client_history");
        }
    } else {
        snprintf(out_path, max_len, "./.tcp_client_history");
    }
#endif

    out_path[max_len - 1] = '\0';
    return out_path;
}

static bool is_builtin_command(const char *buf, const char *cmd) {
    if (!buf || !cmd) {
        return false;
    }

    const char *start = buf;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    size_t cmd_len = strlen(cmd);
    if (strncasecmp(start, cmd, cmd_len) == 0) {
        const char *p = start + cmd_len;
        while (*p && (*p == '\n' || *p == '\r' || isspace((unsigned char)*p))) {
            p++;
        }
        if (*p == '\0') {
            return true;
        }
    }

    return false;
}

static bool is_exit_command(const char *buf) {
    return is_builtin_command(buf, "exit") || is_builtin_command(buf, "quit");
}

static void print_interactive_help(void) {
    printf("Interactive Commands:\n");
    printf("  help     Display this help message\n");
    printf("  status   Display connection parameters and status\n");
    printf("  clear    Clear the terminal screen\n");
    printf("  history  Display command history\n");
    printf("  exit     Disconnect and exit session\n");
    printf("  quit     Disconnect and exit session\n\n");
    printf("Keyboard Shortcuts:\n");
    printf("  Up/Down  Navigate command history\n");
    printf("  Tab      Auto-complete command or payload\n");
    printf("  Ctrl+C   Abort input line / exit\n");
    printf("  Ctrl+D   Exit session on empty input line\n");
    printf("  Ctrl+L   Clear screen\n");
}

static void print_interactive_status(int sockfd, const cli_config_t *config) {
    printf("Connection Status:\n");
    printf("  Host:       %s\n", (config && config->host[0] != '\0') ? config->host : "(unknown)");
    printf("  Port:       %d\n", config ? config->port : 0);
    printf("  Timeout:    %d ms\n", config ? config->timeout_ms : 0);
    printf("  Socket FD:  %d\n", sockfd);
    printf("  Mode:       Interactive (Linenoise TTY)\n");
}

static void print_interactive_history(const char *hist_path) {
    if (!hist_path) {
        return;
    }
    FILE *fp = fopen(hist_path, "r");
    if (!fp) {
        printf("No history recorded.\n");
        return;
    }
    char line[4096];
    int count = 1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }
        if (len > 0) {
            printf("%4d  %s\n", count++, line);
        }
    }
    fclose(fp);
}

static int handle_interactive_command(linenoiseState *l, int sockfd, const cli_config_t *config, const char *hist_path) {
    if (is_exit_command(l->buf)) {
        if (config->verbose) {
            fprintf(stderr, "[VERBOSE] User requested exit.\n");
        }
        return 1; /* signal exit */
    }

    if (is_builtin_command(l->buf, "help")) {
        print_interactive_help();
        linenoiseHistoryAdd(l->buf);
        linenoiseHistorySave(hist_path);
    } else if (is_builtin_command(l->buf, "clear")) {
        linenoiseClearScreen();
        linenoiseHistoryAdd(l->buf);
        linenoiseHistorySave(hist_path);
    } else if (is_builtin_command(l->buf, "status")) {
        print_interactive_status(sockfd, config);
        linenoiseHistoryAdd(l->buf);
        linenoiseHistorySave(hist_path);
    } else if (is_builtin_command(l->buf, "history")) {
        linenoiseHistoryAdd(l->buf);
        linenoiseHistorySave(hist_path);
        print_interactive_history(hist_path);
    } else if (l->len > 0) {
        ssize_t nsent = socket_write_all(sockfd, l->buf, l->len, config->timeout_ms);
        if (nsent < 0) {
            fprintf(stderr, "Error: Failed to transmit data to server.\n");
            return -1; /* network error */
        }
        linenoiseHistoryAdd(l->buf);
        linenoiseHistorySave(hist_path);
    }

    return 0; /* continue */
}

static int handle_socket_data(int sockfd, const cli_config_t *config, char *sock_buf, size_t sock_buf_size, linenoiseState *l) {
    ssize_t nread = socket_read(sockfd, sock_buf, sock_buf_size, config->timeout_ms);
    if (nread > 0) {
        printf("\r\x1b[2K");
        fwrite(sock_buf, 1, (size_t)nread, stdout);
        if (sock_buf[nread - 1] != '\n') {
            putchar('\n');
        }
        fflush(stdout);
        linenoiseEditRedraw(l);
        return 0;
    } else if (nread == 0) {
        if (config->verbose) {
            fprintf(stderr, "[VERBOSE] Server closed connection.\n");
        }
        return 1; /* clean disconnect */
    } else {
        if (nread != SOCKET_ERR_TIMEOUT) {
            fprintf(stderr, "Error: Socket read failed or connection lost.\n");
            return -1; /* network error */
        }
        return 0;
    }
}

#ifdef _WIN32
static bool check_stdin_ready(void) {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE || hStdin == NULL) return false;
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
        if (!PeekNamedPipe(hStdin, NULL, 0, NULL, &avail, NULL)) {
            return true;
        }
        return avail > 0;
    }
}
#endif

/* Fallback non-blocking stream-based line reader for piped STDIN (used by automated tests) */
static int run_interactive_pipe_mode(int sockfd, const cli_config_t *config) {
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

        int poll_timeout = stdin_eof ? 200 : 50;
        int poll_rc = poll_sockets(&spfd, 1, poll_timeout);
        if (poll_rc < 0) {
            int err = get_last_socket_error();
            if (is_socket_wouldblock(err)) continue;
            free(line_buf);
            return 5;
        }

        if (poll_rc == 0 && stdin_eof) {
            free(line_buf);
            return 0;
        }

        if (poll_rc > 0 && (spfd.revents & (POLLHUP | POLLERR))) {
            if (!(spfd.revents & POLLIN)) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] Server hangup / error event detected.\n");
                }
                free(line_buf);
                return 0;
            }
        }

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
            } else {
                if (nread != SOCKET_ERR_TIMEOUT) {
                    fprintf(stderr, "Error: Socket read failed or connection lost.\n");
                    free(line_buf);
                    return 5;
                }
            }
        }

        if (!stdin_eof && check_stdin_ready()) {
            int nread = _read(STDIN_FILENO, raw_stdin, sizeof(raw_stdin));
            if (nread <= 0) {
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
            } else {
                for (int i = 0; i < nread; i++) {
                    char c = raw_stdin[i];
                    if (line_len + 2 > line_cap) {
                        size_t new_cap = (line_cap == 0) ? 4096 : line_cap * 2;
                        if (new_cap < line_len + 2) {
                            new_cap = line_len + 2;
                        }
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

/* Rich interactive mode with linenoise line-editing and 20ms socket multiplexing for TTY */
static int run_interactive_tty_mode(int sockfd, const cli_config_t *config) {
    char hist_path[1024];
    get_interactive_history_path(hist_path, sizeof(hist_path));

    linenoiseHistorySetMaxLen(500);
    linenoiseHistoryLoad(hist_path);
    linenoiseSetCompletionCallback(interactive_completion_callback);

    if (linenoiseEnableRawMode(STDIN_FILENO) == -1) {
        linenoiseHistoryFree();
        return run_interactive_pipe_mode(sockfd, config);
    }

    if (config->verbose) {
        fprintf(stderr, "[VERBOSE] Entering Rich Interactive Mode (TTY). Type 'help' for commands, 'exit' to disconnect.\n");
    }

    char edit_buf[INTERACTIVE_BUF_SIZE];
    linenoiseState l;
    linenoiseEditStart(&l, edit_buf, sizeof(edit_buf), "> ");

    char sock_buf[INTERACTIVE_BUF_SIZE];
    int exit_status = 0;

    while (!signal_handler_is_interrupted()) {
#ifndef _WIN32
        pollfd_t fds[2];
        fds[0].fd = sockfd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        fds[1].fd = STDIN_FILENO;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int poll_rc = poll_sockets(fds, 2, 20);
        if (poll_rc < 0) {
            if (get_last_socket_error() == EINTR) {
                if (signal_handler_is_interrupted()) {
                    break;
                }
                continue;
            }
            fprintf(stderr, "Error: poll() system call failed\n");
            exit_status = 5;
            goto cleanup;
        }

        /* Check socket disconnect / hangup first */
        if (fds[0].revents & (POLLHUP | POLLERR)) {
            if (!(fds[0].revents & POLLIN)) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] Server hangup / error event detected.\n");
                }
                exit_status = 0;
                goto cleanup;
            }
        }

        /* Check socket data arrived */
        if (fds[0].revents & POLLIN) {
            int s_rc = handle_socket_data(sockfd, config, sock_buf, sizeof(sock_buf), &l);
            if (s_rc == 1) {
                exit_status = 0;
                goto cleanup;
            } else if (s_rc == -1) {
                exit_status = 5;
                goto cleanup;
            }
        }

        /* Check STDIN input */
        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
            char ch;
            ssize_t n = read(STDIN_FILENO, &ch, 1);
            if (n <= 0) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] STDIN EOF detected.\n");
                }
                exit_status = 0;
                goto cleanup;
            }

            int c = (unsigned char)ch;
            int feed_rc = linenoiseEditFeed(&l, c);

            while (feed_rc == 0) {
                pollfd_t pfd;
                pfd.fd = STDIN_FILENO;
                pfd.events = POLLIN;
                pfd.revents = 0;
                if (poll_sockets(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLIN)) {
                    break;
                }
                n = read(STDIN_FILENO, &ch, 1);
                if (n <= 0) {
                    break;
                }
                c = (unsigned char)ch;
                feed_rc = linenoiseEditFeed(&l, c);
            }

            if (feed_rc == -1) {
                /* Ctrl+C or Ctrl+D on empty line */
                exit_status = 0;
                goto cleanup;
            }

            if (feed_rc == 1) {
                int cmd_rc = handle_interactive_command(&l, sockfd, config, hist_path);
                if (cmd_rc == 1) {
                    exit_status = 0;
                    goto cleanup;
                } else if (cmd_rc == -1) {
                    exit_status = 5;
                    goto cleanup;
                }
                linenoiseEditStart(&l, edit_buf, sizeof(edit_buf), "> ");
            }
        }
#else
        /* Windows loop using WSAPoll for socket (20ms) and _kbhit() for keyboard */
        pollfd_t spfd;
        spfd.fd = sockfd;
        spfd.events = POLLIN;
        spfd.revents = 0;

        int poll_rc = poll_sockets(&spfd, 1, 20);
        if (poll_rc < 0) {
            int err = get_last_socket_error();
            if (!is_socket_wouldblock(err)) {
                fprintf(stderr, "Error: WSAPoll failed\n");
                exit_status = 5;
                goto cleanup;
            }
        }

        if (poll_rc > 0 && (spfd.revents & (POLLHUP | POLLERR))) {
            if (!(spfd.revents & POLLIN)) {
                if (config->verbose) {
                    fprintf(stderr, "[VERBOSE] Server hangup / error event detected.\n");
                }
                exit_status = 0;
                goto cleanup;
            }
        }

        if (poll_rc > 0 && (spfd.revents & POLLIN)) {
            int s_rc = handle_socket_data(sockfd, config, sock_buf, sizeof(sock_buf), &l);
            if (s_rc == 1) {
                exit_status = 0;
                goto cleanup;
            } else if (s_rc == -1) {
                exit_status = 5;
                goto cleanup;
            }
        }

        while (_kbhit()) {
            int c = _getch();
            int feed_rc = linenoiseEditFeed(&l, c);

            if (feed_rc == -1) {
                /* Ctrl+C or Ctrl+D on empty line */
                exit_status = 0;
                goto cleanup;
            }

            if (feed_rc == 1) {
                int cmd_rc = handle_interactive_command(&l, sockfd, config, hist_path);
                if (cmd_rc == 1) {
                    exit_status = 0;
                    goto cleanup;
                } else if (cmd_rc == -1) {
                    exit_status = 5;
                    goto cleanup;
                }
                linenoiseEditStart(&l, edit_buf, sizeof(edit_buf), "> ");
                break;
            }
        }
#endif
    }

cleanup:
    linenoiseEditStop(&l);
    linenoiseDisableRawMode(STDIN_FILENO);
    linenoiseHistorySave(hist_path);
    linenoiseHistoryFree();
    return exit_status;
}

int run_interactive_mode(int sockfd, const cli_config_t *config) {
    if (sockfd < 0 || !config) {
        return 5;
    }

#ifdef _WIN32
    bool stdin_is_tty = _isatty(_fileno(stdin)) != 0;
#else
    bool stdin_is_tty = isatty(STDIN_FILENO) != 0;
#endif

    if (!stdin_is_tty) {
        return run_interactive_pipe_mode(sockfd, config);
    }

    return run_interactive_tty_mode(sockfd, config);
}
