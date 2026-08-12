#ifndef HSM_DECODER_H
#define HSM_DECODER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t tcp_len;
    bool has_tcp_len;
    char header[64];
    size_t header_len;
    char response_code[3];
    const char *response_name;
    char error_code[3];
    const char *error_description;
    bool is_success;
    const uint8_t *payload;
    size_t payload_len;
    bool has_delimiter;
    char trailer[64];
    size_t trailer_len;
} hsm_response_t;

/**
 * Parses a raw payShield 10K HSM response byte buffer.
 *
 * @param buf Input byte buffer.
 * @param len Buffer length in bytes.
 * @param header_len Configured Message Header length in bytes (default 0 or 4).
 * @param resp Output struct to populate.
 * @return 0 on success, -1 on parse failure.
 */
int hsm_parse_response(const uint8_t *buf, size_t len, size_t header_len, hsm_response_t *resp);

/**
 * Formats parsed HSM response into a human-readable diagnostic analysis string.
 *
 * @param resp Parsed HSM response struct.
 * @param raw_buf Raw input buffer for hex snippet.
 * @param raw_len Raw input buffer length.
 * @param out_str Buffer to write formatted analysis string.
 * @param max_len Capacity of out_str buffer.
 */
void hsm_format_analysis(const hsm_response_t *resp, const uint8_t *raw_buf, size_t raw_len, char *out_str, size_t max_len);

/**
 * Returns human-readable English name for payShield response code.
 */
const char *hsm_lookup_command_name(const char response_code[2]);

/**
 * Returns human-readable error description for payShield 10K error code.
 */
void hsm_lookup_error_code(const char error_code[2], const char **desc);

#endif /* HSM_DECODER_H */
