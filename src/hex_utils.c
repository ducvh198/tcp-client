#include "hex_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int hex_to_bytes(const char *hex_str, uint8_t *out_buf, size_t max_buf_size, size_t *out_len) {
    if (!hex_str || !out_buf || !out_len) {
        return -1;
    }

    *out_len = 0;

    /* Skip leading whitespace */
    while (*hex_str != '\0' && isspace((unsigned char)*hex_str)) {
        hex_str++;
    }

    /* Skip optional "0x" or "0X" prefix */
    if (hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        hex_str += 2;
    }

    size_t count = 0;
    int first_nibble = -1;

    for (size_t i = 0; hex_str[i] != '\0'; i++) {
        char c = hex_str[i];

        /* Skip whitespace delimiters between hex pairs */
        if (isspace((unsigned char)c) || c == ':' || c == '-') {
            continue;
        }

        int val = hex_char_to_int(c);
        if (val < 0) {
            return -1; /* Invalid hex character */
        }

        if (first_nibble < 0) {
            first_nibble = val;
        } else {
            if (count >= max_buf_size) {
                return -2; /* Buffer overflow */
            }
            out_buf[count++] = (uint8_t)((first_nibble << 4) | val);
            first_nibble = -1;
        }
    }

    /* If odd number of hex digits */
    if (first_nibble >= 0) {
        return -1;
    }

    *out_len = count;
    return 0;
}

int bytes_to_hex(const uint8_t *in_buf, size_t in_len, char *out_str, size_t max_out_size, bool uppercase, bool add_spaces) {
    if (!in_buf || !out_str || max_out_size == 0) {
        return -1;
    }

    const char *hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t required = add_spaces ? (in_len * 3) : (in_len * 2);
    if (in_len > 0 && add_spaces) {
        required -= 1; /* No trailing space for last byte */
    }
    required += 1; /* Null terminator */

    if (max_out_size < required) {
        return -1;
    }

    size_t pos = 0;
    for (size_t i = 0; i < in_len; i++) {
        uint8_t byte = in_buf[i];
        out_str[pos++] = hex_chars[(byte >> 4) & 0x0F];
        out_str[pos++] = hex_chars[byte & 0x0F];

        if (add_spaces && i + 1 < in_len) {
            out_str[pos++] = ' ';
        }
    }

    out_str[pos] = '\0';
    return 0;
}

int parse_hex_byte(const char *str, uint8_t *out_byte) {
    if (!str || !out_byte) {
        return -1;
    }

    while (*str != '\0' && isspace((unsigned char)*str)) {
        str++;
    }

    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    if (*str == '\0') {
        return -1;
    }

    int digits[2];
    int num_digits = 0;

    while (*str != '\0' && !isspace((unsigned char)*str)) {
        if (num_digits >= 2) {
            return -1; /* Exceeds 1 byte */
        }
        int val = hex_char_to_int(*str);
        if (val < 0) {
            return -1; /* Invalid hex character */
        }
        digits[num_digits++] = val;
        str++;
    }

    /* Verify remainder is only whitespace */
    while (*str != '\0') {
        if (!isspace((unsigned char)*str)) {
            return -1;
        }
        str++;
    }

    if (num_digits == 1) {
        *out_byte = (uint8_t)digits[0];
    } else if (num_digits == 2) {
        *out_byte = (uint8_t)((digits[0] << 4) | digits[1]);
    } else {
        return -1;
    }

    return 0;
}

int unescape_string(const char *src, uint8_t *out_buf, size_t max_buf_size, size_t *out_len) {
    if (!src || !out_buf || !out_len) {
        return -1;
    }

    size_t count = 0;
    for (size_t i = 0; src[i] != '\0'; i++) {
        if (count >= max_buf_size) {
            return -2; /* Buffer overflow */
        }

        if (src[i] == '\\' && src[i + 1] != '\0') {
            char next = src[i + 1];
            if (next == 'x' || next == 'X') {
                if (src[i + 2] != '\0' && src[i + 3] != '\0') {
                    int h1 = hex_char_to_int(src[i + 2]);
                    int h2 = hex_char_to_int(src[i + 3]);
                    if (h1 >= 0 && h2 >= 0) {
                        out_buf[count++] = (uint8_t)((h1 << 4) | h2);
                        i += 3;
                        continue;
                    }
                }
                return -1; /* Malformed \xHH sequence */
            } else if (next == 'r') {
                out_buf[count++] = '\r';
                i++;
            } else if (next == 'n') {
                out_buf[count++] = '\n';
                i++;
            } else if (next == 't') {
                out_buf[count++] = '\t';
                i++;
            } else if (next == '0') {
                out_buf[count++] = '\0';
                i++;
            } else if (next == '\\') {
                out_buf[count++] = '\\';
                i++;
            } else {
                out_buf[count++] = (uint8_t)next;
                i++;
            }
        } else {
            out_buf[count++] = (uint8_t)src[i];
        }
    }

    *out_len = count;
    return 0;
}
