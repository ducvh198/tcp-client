#include "hsm_decoder.h"
#include "hex_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

typedef struct {
    const char code[3];
    const char *en_desc;
} hsm_error_entry_t;

static const hsm_error_entry_t ERROR_TABLE[] = {
    {"00", "No error"},
    {"01", "Verification failure (PIN/CVV/MAC)"},
    {"02", "Key inappropriate length / Hash validation failure"},
    {"03", "Invalid message type / Secret key / Zero PIN block"},
    {"04", "Invalid key type code / Public key encoding error"},
    {"05", "Invalid key length flag / Hash ID / Odd input pairs"},
    {"06", "Invalid signature ID / Public key algorithm ID"},
    {"07", "Public exponent length error / Key length mismatch"},
    {"08", "Invalid public exponent"},
    {"09", "Secret key error"},
    {"10", "Source key parity error (ZMK, TPK, BDK)"},
    {"11", "Destination key parity error or key all zeros"},
    {"12", "User storage not available"},
    {"13", "Invalid LMK Identifier"},
    {"14", "PIN encrypted under LMK pair 02-03 invalid"},
    {"15", "Invalid input data format / Missing delimiter"},
    {"16", "Console or printer not ready"},
    {"17", "HSM not authorized or operation prohibited"},
    {"18", "Document format definition not loaded"},
    {"19", "Specified Diebold table invalid"},
    {"20", "PIN block does not contain valid values"},
    {"21", "Invalid index value or overflow"},
    {"22", "Invalid account number"},
    {"23", "Invalid PIN block format code / PCI HSM limitation"},
    {"24", "PIN is fewer than 4 or more than 12 digits"},
    {"25", "Decimalization Table error"},
    {"26", "Invalid key scheme indicator"},
    {"27", "Incompatible key length"},
    {"28", "Invalid key type"},
    {"29", "Key function not permitted / Key separation violation"},
    {"30", "Invalid reference number"},
    {"31", "Insufficient solicitation entries for batch"},
    {"32", "AES not licensed"},
    {"33", "LMK key change storage corrupted"},
    {"40", "Invalid firmware checksum"},
    {"41", "Internal hardware/software error"},
    {"42", "DES failure"},
    {"43", "RSA Key Generation failure"},
    {"47", "Hardware failure"},
    {"51", "Invalid message header"},
    {"65", "Transaction Key Scheme set to None"},
    {"67", "Command not licensed"},
    {"68", "Command disabled in security settings"},
    {"69", "PIN block format disabled"},
    {"80", "Data length error"},
    {"A1", "Incompatible LMK schemes"},
    {"A2", "Incompatible LMK identifiers"},
    {"A3", "Incompatible key block LMK identifiers"},
    {"A4", "Key block authentication failure"},
    {"A5", "Incompatible key length in Key Block"},
    {"A6", "TR-31: Invalid key usage attribute"},
    {"A7", "TR-31: Invalid algorithm attribute"},
    {"A8", "TR-31: Invalid mode of use attribute"},
    {"A9", "TR-31: Invalid key version number"},
    {"AA", "TR-31: Invalid export field"},
    {"AB", "TR-31: Invalid number of optional blocks"},
    {"BB", "TR-31: Invalid wrapping key (ZMK)"},
    {"BC", "TR-31: Repeated optional block"},
    {""  , NULL}
};

const char *hsm_lookup_command_name(const char response_code[2]) {
    if (!response_code) return "Unknown Response Code";
    if (response_code[0] == 'A' && response_code[1] == '1') return "Generate a Key Response";
    if (response_code[0] == 'N' && response_code[1] == 'D') return "Diagnostics Response";
    if (response_code[0] == 'C' && response_code[1] == 'C') return "Generate MAC Response";
    if (response_code[0] == 'E' && response_code[1] == 'F') return "Translate PIN Block Response";
    if (response_code[0] == 'F' && response_code[1] == 'B') return "Generate CVV Response";
    if (response_code[0] == 'D' && response_code[1] == 'H') return "Generate Dynamic CVV Response";
    if (response_code[0] == 'C' && response_code[1] == 'B') return "Cancel Command Response";
    if (response_code[0] == 'B' && response_code[1] == 'V') return "Generate RSA Key Pair Response";
    if (response_code[0] == 'N' && response_code[1] == '1') return "Generate HMK Key Response";
    return "Unknown Response Code";
}

void hsm_lookup_error_code(const char error_code[2], const char **desc) {
    if (!desc) return;
    if (!error_code) {
        *desc = "Unknown Error";
        return;
    }

    for (size_t i = 0; ERROR_TABLE[i].code[0] != '\0'; i++) {
        if (ERROR_TABLE[i].code[0] == error_code[0] && ERROR_TABLE[i].code[1] == error_code[1]) {
            *desc = ERROR_TABLE[i].en_desc;
            return;
        }
    }

    *desc = "Unrecognized HSM Error Code";
}

int hsm_parse_response(const uint8_t *buf, size_t len, size_t header_len, hsm_response_t *resp) {
    if (!buf || len == 0 || !resp) {
        return -1;
    }

    memset(resp, 0, sizeof(hsm_response_t));

    size_t pos = 0;

    /* Check for 2-byte TCP/IP Big-Endian Length Indicator header */
    if (len >= 2) {
        uint16_t tcp_len = (uint16_t)((buf[0] << 8) | buf[1]);
        /* If 2 + tcp_len matches total packet length or looks like TCP frame */
        if (tcp_len == len - 2 || (len > 4 && tcp_len <= len)) {
            resp->has_tcp_len = true;
            resp->tcp_len = tcp_len;
            pos += 2;
        }
    }

    size_t remain = len - pos;

    /* Message Header extraction (default header_len if present) */
    if (header_len > 0 && remain >= header_len) {
        size_t copy_h = (header_len < sizeof(resp->header) - 1) ? header_len : sizeof(resp->header) - 1;
        memcpy(resp->header, buf + pos, copy_h);
        resp->header[copy_h] = '\0';
        resp->header_len = copy_h;
        pos += header_len;
        remain -= header_len;
    }

    /* Response Code (2 ASCII characters, e.g. "ND", "A1", "CB", "CX") */
    if (remain >= 2) {
        resp->response_code[0] = (char)buf[pos];
        resp->response_code[1] = (char)buf[pos + 1];
        resp->response_code[2] = '\0';
        pos += 2;
        remain -= 2;
    } else {
        return -1; /* Packet too short for Response Code */
    }

    /* Error Code (2 ASCII/AN characters, e.g. "00", "01", "10", "A6") */
    if (remain >= 2) {
        resp->error_code[0] = (char)buf[pos];
        resp->error_code[1] = (char)buf[pos + 1];
        resp->error_code[2] = '\0';
        pos += 2;
        remain -= 2;
    } else {
        return -1; /* Packet too short for Error Code */
    }

    resp->is_success = (resp->error_code[0] == '0' && resp->error_code[1] == '0');
    resp->response_name = hsm_lookup_command_name(resp->response_code);
    hsm_lookup_error_code(resp->error_code, &resp->error_description);

    /* Response Payload Data */
    if (remain > 0) {
        resp->payload = buf + pos;
        resp->payload_len = remain;

        /* Check for End Message Delimiter 0x19 (EM) */
        for (size_t i = 0; i < remain; i++) {
            if (buf[pos + i] == 0x19) {
                resp->has_delimiter = true;
                resp->payload_len = i; /* Payload ends before delimiter */
                size_t trailer_pos = pos + i + 1;
                if (trailer_pos < len) {
                    size_t trailer_rem = len - trailer_pos;
                    size_t copy_t = (trailer_rem < sizeof(resp->trailer) - 1) ? trailer_rem : sizeof(resp->trailer) - 1;
                    memcpy(resp->trailer, buf + trailer_pos, copy_t);
                    resp->trailer[copy_t] = '\0';
                    resp->trailer_len = copy_t;
                }
                break;
            }
        }
    }

    return 0;
}

static void append_breakdown_line(char *buf, size_t buf_size, size_t *off, const char *fmt, ...) {
    if (!buf || !off || *off >= buf_size) return;
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + *off, buf_size - *off, fmt, args);
    va_end(args);
    if (written > 0) {
        *off += (size_t)written;
        if (*off >= buf_size) {
            *off = buf_size - 1;
        }
    }
}

void hsm_format_analysis(const hsm_response_t *resp, const uint8_t *raw_buf, size_t raw_len, char *out_str, size_t max_len) {
    if (!resp || !out_str || max_len == 0) {
        return;
    }

    char hex_dump[512];
    hex_dump[0] = '\0';
    if (raw_buf && raw_len > 0) {
        bytes_to_hex(raw_buf, (raw_len > 64) ? 64 : raw_len, hex_dump, sizeof(hex_dump), true, true);
        if (raw_len > 64) {
            strcat(hex_dump, " ...");
        }
    }

    char payload_hex[512];
    payload_hex[0] = '\0';
    char payload_ascii[256];
    payload_ascii[0] = '\0';

    if (resp->payload && resp->payload_len > 0) {
        bytes_to_hex(resp->payload, (resp->payload_len > 64) ? 64 : resp->payload_len, payload_hex, sizeof(payload_hex), true, true);
        if (resp->payload_len > 64) {
            strcat(payload_hex, " ...");
        }

        size_t ascii_len = (resp->payload_len < sizeof(payload_ascii) - 1) ? resp->payload_len : sizeof(payload_ascii) - 1;
        for (size_t i = 0; i < ascii_len; i++) {
            char c = (char)resp->payload[i];
            payload_ascii[i] = (isprint((unsigned char)c)) ? c : '.';
        }
        payload_ascii[ascii_len] = '\0';
    }

    /* Format DETAILED FIELD BREAKDOWN section */
    char breakdown[1024];
    size_t b_off = 0;
    breakdown[0] = '\0';

    append_breakdown_line(breakdown, sizeof(breakdown), &b_off, "DETAILED FIELD BREAKDOWN:\n");

    if (resp->has_tcp_len) {
        append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
            "TCP/IP Header............ = [%04X] %u Bytes\n",
            (unsigned int)resp->tcp_len, (unsigned int)resp->tcp_len);
    }

    if (resp->header_len > 0) {
        append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
            "Message Header........... = [%s]\n",
            resp->header);
    }

    append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
        "Command Code............. = [%s] %s\n",
        resp->response_code[0] ? resp->response_code : "N/A",
        resp->response_name ? resp->response_name : "Unknown Response Code");

    append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
        "Error Code............... = [%s] %s\n",
        resp->error_code[0] ? resp->error_code : "N/A",
        resp->error_description ? resp->error_description : "Unknown Error");

    if (resp->is_success) {
        if (resp->response_code[0] == 'A' && resp->response_code[1] == '1') {
            if (resp->payload && resp->payload_len > 0) {
                char scheme = (char)resp->payload[0];
                if (scheme == 'U' || scheme == 'T' || scheme == 'X' || scheme == 'Z' || scheme == 'S' || scheme == 'Y') {
                    size_t rem = resp->payload_len;
                    size_t key_len = rem;
                    size_t kcv_len = 0;

                    if (rem >= 65) {
                        key_len = 65;
                        kcv_len = rem - 65;
                    } else if (rem >= 49) {
                        key_len = 49;
                        kcv_len = rem - 49;
                    } else if (rem >= 33) {
                        key_len = 33;
                        kcv_len = rem - 33;
                    } else if (rem >= 17) {
                        key_len = 17;
                        kcv_len = rem - 17;
                    }

                    append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
                        "Key...................... = [%.*s]\n",
                        (int)key_len, (const char *)resp->payload);

                    if (kcv_len > 0) {
                        append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
                            "Key Check Value (KCV).... = [%.*s]\n",
                            (int)kcv_len, (const char *)(resp->payload + key_len));
                    }
                } else {
                    append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
                        "Key Data................. = [%.*s]\n",
                        (int)resp->payload_len, (const char *)resp->payload);
                }
            }
        } else {
            if (resp->payload && resp->payload_len > 0) {
                append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
                    "Data Payload............. = [%.*s]\n",
                    (int)resp->payload_len, (const char *)resp->payload);
            }
        }
    } else {
        append_breakdown_line(breakdown, sizeof(breakdown), &b_off,
            "Error Details............ = [%s]\n",
            resp->error_description ? resp->error_description : "Unknown Error");
    }

    snprintf(out_str, max_len,
        "======================================================================\n"
        "             payShield 10K HSM RESPONSE DECODER ANALYSIS              \n"
        "======================================================================\n"
        "Raw Packet Length : %lu bytes\n"
        "Raw Hex Packet    : %s\n"
        "----------------------------------------------------------------------\n"
        "TCP Length Header : %s\n"
        "Message Header    : %s\n"
        "Response Code     : '%s' -> [%s]\n"
        "Error Code        : '%s' -> [%s]\n"
        "                    Error Description: %s\n"
        "----------------------------------------------------------------------\n"
        "%s"
        "----------------------------------------------------------------------\n"
        "Response Payload  : %lu bytes %s\n"
        "%s%s%s%s%s"
        "======================================================================\n",
        (unsigned long)raw_len,
        hex_dump[0] ? hex_dump : "(None)",
        resp->has_tcp_len ? "2 Bytes (Binary Big-Endian)" : "None / Omitted",
        resp->header_len > 0 ? resp->header : "(None)",
        resp->response_code[0] ? resp->response_code : "N/A",
        resp->response_name ? resp->response_name : "Unknown Response Code",
        resp->error_code[0] ? resp->error_code : "N/A",
        resp->is_success ? "SUCCESS / OK" : "ERROR / FAILED",
        resp->error_description ? resp->error_description : "",
        breakdown,
        (unsigned long)resp->payload_len,
        resp->is_success ? "" : "(Truncated by HSM due to error)",
        (resp->payload_len > 0) ? "Payload HEX       : " : "",
        (resp->payload_len > 0) ? payload_hex : "",
        (resp->payload_len > 0) ? "\nPayload ASCII     : " : "",
        (resp->payload_len > 0) ? payload_ascii : "",
        (resp->payload_len > 0) ? "\n" : ""
    );
}
