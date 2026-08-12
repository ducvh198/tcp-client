#include <stdio.h>
#include <stdlib.h>
#include "compat.h"
#include "cli_args.h"
#include "socket_client.h"
#include "mode_interactive.h"
#include "mode_oneshot.h"
#include "signal_handler.h"

int main(int argc, char *argv[]) {
    if (platform_init() != 0) {
        return 5;
    }

    setup_signal_handlers();

    cli_config_t config;
    int parse_res = parse_cli_args(argc, argv, &config);
    if (parse_res != 0) {
        platform_cleanup();
        return 1;
    }

    if (config.show_help) {
        print_usage(argv[0]);
        platform_cleanup();
        return 0;
    }

    if (config.show_version) {
        print_version();
        platform_cleanup();
        return 0;
    }

    if (config.mode == MODE_AUTO) {
        if (config.is_hex || config.is_ascii) {
            config.mode = MODE_ONESHOT;
        } else {
#ifdef _WIN32
            bool stdin_is_tty = _isatty(_fileno(stdin)) != 0;
#else
            bool stdin_is_tty = isatty(STDIN_FILENO) != 0;
#endif
            config.mode = (stdin_is_tty || config.force_interactive) ? MODE_INTERACTIVE : MODE_ONESHOT;
        }
    }

    int sockfd = socket_connect(config.host, config.port, config.timeout_ms, config.verbose);
    if (sockfd < 0) {
        int exit_code = -sockfd;
        switch (exit_code) {
        case 2:
            fprintf(stderr, "Error: Host resolution failed for '%s'\n", config.host);
            break;
        case 3:
            fprintf(stderr, "Error: Connection to %s:%d refused or target host unreachable\n", config.host, config.port);
            break;
        case 4:
            fprintf(stderr, "Error: Connection attempt to %s:%d timed out after %d ms\n", config.host, config.port, config.timeout_ms);
            break;
        case 5:
        default:
            fprintf(stderr, "Error: Network I/O error on %s:%d\n", config.host, config.port);
            break;
        }
        platform_cleanup();
        return exit_code;
    }

    if (config.verbose) {
        fprintf(stderr, "[VERBOSE] Connected successfully to %s:%d (sockfd: %d, mode: %s)\n",
                config.host, config.port, sockfd,
                (config.mode == MODE_INTERACTIVE) ? "interactive" : "oneshot");
    }

    int result_code = 0;
    if (config.mode == MODE_INTERACTIVE) {
        result_code = run_interactive_mode(sockfd, &config);
    } else {
        result_code = run_oneshot_mode(sockfd, &config);
    }

    socket_close(sockfd);
    platform_cleanup();
    return result_code;
}
