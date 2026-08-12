#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Parses a hex string (e.g. "00 06 30 30", "00063030", "0x0006") into a byte array.
 * Ignores spaces, tabs, newlines, and optional "0x"/"0X" prefix.
 *
 * @param hex_str Input null-terminated hex string.
 * @param out_buf Buffer to store output bytes.
 * @param max_buf_size Maximum byte capacity of out_buf.
 * @param out_len Pointer to store resulting byte length.
 * @return 0 on success, -1 on invalid hex character or odd length, -2 if out_buf capacity exceeded.
 */
int hex_to_bytes(const char *hex_str, uint8_t *out_buf, size_t max_buf_size, size_t *out_len);

/**
 * Formats a byte array into a hex string representation.
 *
 * @param in_buf Input byte array.
 * @param in_len Length of in_buf in bytes.
 * @param out_str Buffer to store output null-terminated hex string.
 * @param max_out_size Maximum capacity of out_str buffer.
 * @param uppercase If true, formats hex digits in uppercase (0-9, A-F), else lowercase (0-9, a-f).
 * @param add_spaces If true, separates bytes with spaces (e.g. "00 06 30"), else compact ("000630").
 * @return 0 on success, -1 on invalid arguments or output buffer overflow.
 */
int bytes_to_hex(const uint8_t *in_buf, size_t in_len, char *out_str, size_t max_out_size, bool uppercase, bool add_spaces);

#endif /* HEX_UTILS_H */
