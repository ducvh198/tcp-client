# Design Spec: payShield 10K HSM Detailed Field Decoding

## Overview
Enhance the existing HSM response decoder in `tcp-client-cli` (`--decode-hsm`) to perform detailed field breakdown of incoming payShield 10K HSM response packets, presenting parsed fields in aligned Key-Value format (e.g., `Field............ = [Value] Description`). All output strings, error descriptions, and field labels are strictly in English.

## Requirements & Scope

### 1. English-Only Localization
- Remove all Vietnamese translations (`vi_desc`) from the HSM decoder data structures and output text.
- Simplify `hsm_error_entry_t` and `hsm_response_t` to hold only English descriptions.
- Output all analysis headers, error descriptions, and field labels exclusively in English.

### 2. Command Code Lookup Table
- Implement a command lookup engine for payShield 10K response codes (ASCII 2 chars).
- Map common payShield response codes to human-readable English command names:
  - `A1`: `Generate a Key Response`
  - `ND`: `Diagnostics Response`
  - `CC`: `Generate MAC Response`
  - `EF`: `Translate PIN Block Response`
  - `FB`: `Generate CVV Response`
  - `DH`: `Generate Dynamic CVV Response`
  - `CB`: `Cancel Command Response`
  - `BV`: `Generate RSA Key Pair Response`
  - `N1`: `Generate HMK Key Response`
  - Default fallback for unlisted response codes: `Unknown Response Code`

### 3. Detailed Field Breakdown Section
- Add a new section `DETAILED FIELD BREAKDOWN:` to `hsm_format_analysis()`.
- Format fields with 25-character dot padding: `Field Name............ = [Value] Description`
- Standard fields printed when present:
  - `TCP/IP Header............ = [<HEX>] <N> Bytes` (when 2-byte TCP len header is present)
  - `Message Header........... = [<HEX>]` (when header_len > 0)
  - `Command Code............. = [<CODE>] <Command Name>`
  - `Error Code............... = [<ERR>] <Error Description>`
- Specialized Payload field extraction (when Error Code == "00"):
  - For `A1` (Generate Key Response):
    - `Key...................... = [<KEY_STRING>]`
    - `Key Check Value (KCV).... = [<KCV_STRING>]` (if extra bytes present after key)
  - Generic payload fallback (for commands without specific field extractors):
    - `Data Payload............ = [<HEX_OR_ASCII>]`

## Data Structures & Function Signature Changes

### In `src/hsm_decoder.h`
```c
typedef struct {
    uint16_t tcp_len;
    bool has_tcp_len;
    char header[64];
    size_t header_len;
    char response_code[3];
    const char *response_name; /* Human-readable English command name */
    char error_code[3];
    const char *error_description; /* English error description */
    bool is_success;
    const uint8_t *payload;
    size_t payload_len;
    bool has_delimiter;
    char trailer[64];
    size_t trailer_len;
} hsm_response_t;

/**
 * Returns human-readable English name for payShield response code.
 */
const char *hsm_lookup_command_name(const char response_code[2]);
```

### In `src/hsm_decoder.c`
- Update `ERROR_TABLE` struct to drop `vi_desc`.
- Update `hsm_lookup_error_code()` signature to return `const char **desc`.
- Update `hsm_parse_response()` to populate `resp->response_name` via `hsm_lookup_command_name()`.
- Update `hsm_format_analysis()` to output the `DETAILED FIELD BREAKDOWN:` section formatted in 100% English.

## Output Format Example
```text
======================================================================
             payShield 10K HSM RESPONSE DECODER ANALYSIS              
======================================================================
Raw Packet Length : 47 bytes
Raw Hex Packet    : 00 2D 30 30 30 30 30 30 30 30 41 31 30 30 ...
----------------------------------------------------------------------
TCP Length Header : 2 Bytes (Binary Big-Endian)
Message Header    : 00000000
Response Code     : 'A1' -> [Generate a Key Response]
Error Code        : '00' -> [No error]
----------------------------------------------------------------------
DETAILED FIELD BREAKDOWN:
TCP/IP Header............ = [002D] 45 Bytes
Message Header........... = [00000000]
Command Code............. = [A1] Generate a Key Response
Error Code............... = [00] No error
Key...................... = [U946EC35A217E415D333AFAC7EC336116]
Key Check Value (KCV).... = [A1B2C3]
----------------------------------------------------------------------
Response Payload  : 41 bytes
Payload HEX       : 55 39 34 36 45 43 33 35 41 32 31 37 45 34 ...
Payload ASCII     : U946EC35A217E4...
======================================================================
```

## Verification Plan
1. Rebuild binary with `make` clean and build.
2. Run mock server or test inputs against `--decode-hsm` to verify formatted English breakdown.
3. Run existing test suite (`python tests/test_runner.py`) to confirm no regressions in CLI core features.
