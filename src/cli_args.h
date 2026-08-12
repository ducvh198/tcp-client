#ifndef CLI_ARGS_H
#define CLI_ARGS_H

#include <stdbool.h>

typedef enum {
    MODE_AUTO = 0,
    MODE_INTERACTIVE,
    MODE_ONESHOT
} client_mode_t;

typedef struct {
    char host[256];
    int port;
    int timeout_ms;
    bool force_interactive;
    bool verbose;
    bool show_help;
    bool show_version;
    client_mode_t mode;
    char hex_payload[65536];
    bool is_hex;
    bool hex_out;
    char ascii_payload[65536];
    bool is_ascii;
    bool add_tcp_len;
    bool decode_hsm;
    int hsm_header_len;
} cli_config_t;

int parse_cli_args(int argc, char *argv[], cli_config_t *config);
void print_usage(const char *prog_name);
void print_version(void);

#endif /* CLI_ARGS_H */
