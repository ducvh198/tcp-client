#include "cli_args.h"
#include "hex_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#define DEFAULT_TIMEOUT_MS 5000
#define APP_VERSION "1.0.0"

static bool parse_int(const char *str, int *out_val) {
    if (!str || *str == '\0') {
        return false;
    }
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || *endptr != '\0') {
        return false;
    }
    if (val < INT_MIN || val > INT_MAX) {
        return false;
    }
    *out_val = (int)val;
    return true;
}

void print_usage(const char *prog_name) {
    const char *name = (prog_name && prog_name[0] != '\0') ? prog_name : "./tcp-client";
    printf("Usage: %s [options] <host> <port>\n", name);
    printf("       %s [options] --host <host> --port <port>\n\n", name);
    printf("Options:\n");
    printf("  -h, --host <host>       Target server hostname or IP address\n");
    printf("  -p, --port <port>       Target server port (1-65535)\n");
    printf("  -t, --timeout <ms>      Connection/IO timeout in milliseconds (default: 5000)\n");
    printf("  -x, --hex <hex_string>  Send raw HEX payload string directly (e.g. \"00 06 30 30 30 30\")\n");
    printf("  -X, --hex-out           Display server response formatted in HEX\n");
    printf("  -a, --ascii <str>       Send raw ASCII payload string directly (e.g. \"NC0000\")\n");
    printf("  -L, --add-tcp-len       Prepend 2-byte Big-Endian TCP length header to ASCII/HEX payload\n");
    printf("  -D, --decode-hsm        Enable payShield 10K HSM Response Decoder analysis\n");
    printf("      --hsm-header-len <n> Set HSM Message Header length in bytes (default: 0 or 4)\n");
    printf("  -i, --interactive       Force interactive mode\n");
    printf("  -v, --verbose           Enable verbose output\n");
    printf("  -H, --help              Display this help message and exit\n");
    printf("  -V, --version           Output version information and exit\n");
}

void print_version(void) {
    printf("tcp-client version %s\n", APP_VERSION);
}

int parse_cli_args(int argc, char *argv[], cli_config_t *config) {
    if (!config) {
        return 1;
    }

    memset(config, 0, sizeof(cli_config_t));
    config->timeout_ms = DEFAULT_TIMEOUT_MS;
    config->mode = MODE_AUTO;

    optind = 1; /* Reset getopt internal state */
    opterr = 0; /* Disable default getopt error messages for custom formatting */

    static struct option long_options[] = {
        {"host",           required_argument, NULL, 'h'},
        {"port",           required_argument, NULL, 'p'},
        {"timeout",        required_argument, NULL, 't'},
        {"hex",            required_argument, NULL, 'x'},
        {"hex-out",        no_argument,       NULL, 'X'},
        {"ascii",          required_argument, NULL, 'a'},
        {"add-tcp-len",    no_argument,       NULL, 'L'},
        {"decode-hsm",     no_argument,       NULL, 'D'},
        {"hsm-header-len", required_argument, NULL, 1000},
        {"interactive",    no_argument,       NULL, 'i'},
        {"verbose",        no_argument,       NULL, 'v'},
        {"help",           no_argument,       NULL, 'H'},
        {"version",        no_argument,       NULL, 'V'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:t:x:Xa:LDivHV", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            if (strlen(optarg) >= sizeof(config->host)) {
                fprintf(stderr, "Error: Host string too long (max %lu characters)\n", (unsigned long)(sizeof(config->host) - 1));
                return 1;
            }
            strncpy(config->host, optarg, sizeof(config->host) - 1);
            config->host[sizeof(config->host) - 1] = '\0';
            break;

        case 'p': {
            int port_val;
            if (!parse_int(optarg, &port_val) || port_val < 1 || port_val > 65535) {
                fprintf(stderr, "Error: Invalid port number '%s'. Port must be an integer between 1 and 65535.\n", optarg);
                return 1;
            }
            config->port = port_val;
            break;
        }

        case 't': {
            int timeout_val;
            if (!parse_int(optarg, &timeout_val) || timeout_val < 0) {
                fprintf(stderr, "Error: Invalid timeout value '%s'. Timeout must be a non-negative integer.\n", optarg);
                return 1;
            }
            config->timeout_ms = timeout_val;
            break;
        }

        case 'x':
            if (strlen(optarg) >= sizeof(config->hex_payload)) {
                fprintf(stderr, "Error: HEX payload too long (max %lu characters)\n", (unsigned long)(sizeof(config->hex_payload) - 1));
                return 1;
            }
            {
                uint8_t dummy_buf[65536];
                size_t dummy_len = 0;
                if (hex_to_bytes(optarg, dummy_buf, sizeof(dummy_buf), &dummy_len) != 0) {
                    fprintf(stderr, "Error: Invalid HEX string format '%s'. Ensure even length and valid hex characters (0-9, a-f, A-F).\n", optarg);
                    return 1;
                }
            }
            strncpy(config->hex_payload, optarg, sizeof(config->hex_payload) - 1);
            config->hex_payload[sizeof(config->hex_payload) - 1] = '\0';
            config->is_hex = true;
            break;

        case 'X':
            config->hex_out = true;
            break;

        case 'a':
            if (strlen(optarg) >= sizeof(config->ascii_payload)) {
                fprintf(stderr, "Error: ASCII payload too long (max %lu characters)\n", (unsigned long)(sizeof(config->ascii_payload) - 1));
                return 1;
            }
            strncpy(config->ascii_payload, optarg, sizeof(config->ascii_payload) - 1);
            config->ascii_payload[sizeof(config->ascii_payload) - 1] = '\0';
            config->is_ascii = true;
            break;

        case 'L':
            config->add_tcp_len = true;
            break;

        case 'D':
            config->decode_hsm = true;
            break;

        case 1000: { /* --hsm-header-len */
            int len_val;
            if (!parse_int(optarg, &len_val) || len_val < 0 || len_val > 64) {
                fprintf(stderr, "Error: Invalid HSM header length '%s'. Must be an integer between 0 and 64.\n", optarg);
                return 1;
            }
            config->hsm_header_len = len_val;
            break;
        }

        case 'i':
            config->force_interactive = true;
            config->mode = MODE_INTERACTIVE;
            break;

        case 'v':
            config->verbose = true;
            break;

        case 'H':
            config->show_help = true;
            return 0;

        case 'V':
            config->show_version = true;
            return 0;

        case '?':
            if (optopt == 'h' || optopt == 'p' || optopt == 't' || optopt == 'x') {
                fprintf(stderr, "Error: Option '-%c' requires an argument.\n", optopt);
            } else {
                fprintf(stderr, "Error: Unrecognized option or invalid argument.\n");
            }
            return 1;

        default:
            return 1;
        }
    }

    /* Process positional arguments for host and port if not specified via flags */
    while (optind < argc) {
        if (config->host[0] == '\0') {
            if (strlen(argv[optind]) >= sizeof(config->host)) {
                fprintf(stderr, "Error: Host string too long (max %lu characters)\n", (unsigned long)(sizeof(config->host) - 1));
                return 1;
            }
            strncpy(config->host, argv[optind], sizeof(config->host) - 1);
            config->host[sizeof(config->host) - 1] = '\0';
        } else if (config->port == 0) {
            int port_val;
            if (!parse_int(argv[optind], &port_val) || port_val < 1 || port_val > 65535) {
                fprintf(stderr, "Error: Invalid port number '%s'. Port must be an integer between 1 and 65535.\n", argv[optind]);
                return 1;
            }
            config->port = port_val;
        } else {
            fprintf(stderr, "Error: Unexpected argument '%s'\n", argv[optind]);
            return 1;
        }
        optind++;
    }

    /* Validate required parameters */
    if (config->show_help || config->show_version) {
        return 0;
    }

    if (config->host[0] == '\0') {
        fprintf(stderr, "Error: Host parameter is required.\n");
        return 1;
    }

    if (config->port == 0) {
        fprintf(stderr, "Error: Port parameter is required.\n");
        return 1;
    }

    return 0;
}
