# payShield 10K HSM Detailed Field Decoding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enhance the HSM decoder in `tcp-client-cli` (`--decode-hsm`) to output a detailed field breakdown in aligned Key-Value format (`Field............ = [Value] Description`) in 100% English.

**Architecture:** Update `src/hsm_decoder.h` and `src/hsm_decoder.c` to remove all Vietnamese string constants (`vi_desc`), introduce `hsm_lookup_command_name()` to map response codes to English command names, and format payload fields into a structured `DETAILED FIELD BREAKDOWN:` section in `hsm_format_analysis()`.

**Tech Stack:** C99 / POSIX socket CLI, Make build system, Python E2E test suite.

## Global Constraints
- Language & Build: C99, compiled with `gcc -Wall -Wextra -O2`.
- Localization: Strictly 100% English text. Remove all Vietnamese strings.
- Output Alignment: Field breakdown formatted with 25-character dot padding `Field Name............ = [Value] Description`.

---

### Task 1: English Localization & Command Lookup Table Engine

**Files:**
- Modify: `src/hsm_decoder.h`
- Modify: `src/hsm_decoder.c`

**Interfaces:**
- Consumes: Raw payload buffer and parsed HSM header/error code.
- Produces: `const char *hsm_lookup_command_name(const char response_code[2])` returning human-readable English command name.

- [ ] **Step 1: Update `src/hsm_decoder.h` struct definitions & declarations**

Remove `error_description_vi` from `hsm_response_t` and add `response_name` and `error_description`:
```c
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

const char *hsm_lookup_command_name(const char response_code[2]);
```

- [ ] **Step 2: Update `src/hsm_decoder.c` to strip Vietnamese descriptions and implement command lookup**

In `src/hsm_decoder.c`:
1. Remove `vi_desc` from `hsm_error_entry_t` table entries.
2. Implement `hsm_lookup_command_name()` mapping response codes (`A1`, `ND`, `CC`, `EF`, `FB`, `DH`, `CB`, `BV`, `N1`, etc.) to English descriptions.
3. Update `hsm_lookup_error_code()` to output only English descriptions.
4. Update `hsm_parse_response()` to populate `resp->response_name = hsm_lookup_command_name(resp->response_code)`.

- [ ] **Step 3: Build to verify compilation**

Run: `make clean && make`
Expected output: Successful build producing `tcp-client` without warnings or errors.

- [ ] **Step 4: Commit Task 1**

```bash
git add src/hsm_decoder.h src/hsm_decoder.c
git commit -m "refactor: convert HSM decoder to 100% English and add command lookup table"
```

---

### Task 2: Detailed Field Breakdown Extractor & Formatter

**Files:**
- Modify: `src/hsm_decoder.c:179-247`

**Interfaces:**
- Consumes: Parsed `hsm_response_t` struct.
- Produces: Formatted English analysis text with `DETAILED FIELD BREAKDOWN:` block in `hsm_format_analysis()`.

- [ ] **Step 1: Implement `DETAILED FIELD BREAKDOWN:` output in `hsm_format_analysis()`**

Update `hsm_format_analysis()` in `src/hsm_decoder.c` to construct aligned Key-Value fields:
```c
/* Detailed Field Breakdown output */
snprintf(field_breakdown, sizeof(field_breakdown),
    "DETAILED FIELD BREAKDOWN:\n"
    "%s" /* TCP/IP Header if present */
    "%s" /* Message Header if present */
    "Command Code............. = [%s] %s\n"
    "Error Code............... = [%s] %s\n"
    "%s" /* Payload fields (Key, KCV, Payload Data) */,
    ...
);
```

For command `A1` (Generate Key Response) when `is_success` is true, parse:
- `Key...................... = [<Key ASCII>]`
- `Key Check Value (KCV).... = [<KCV ASCII>]` (if present)

- [ ] **Step 2: Build and manual verification**

Run: `make clean && make`
Expected output: Clean build of `tcp-client`.

- [ ] **Step 3: Test with sample HSM payload**

Run a command piping a sample `A1` response through `--decode-hsm`:
Run: `echo -ne "\x00\x2d00000000A100U946EC35A217E415D333AFAC7EC336116123456" | ./tcp-client --decode-hsm 127.0.0.1 9999 || true`
Expected output: Contains `DETAILED FIELD BREAKDOWN:` with `TCP/IP Header`, `Command Code`, `Error Code`, `Key`, and `Key Check Value (KCV)`.

- [ ] **Step 4: Commit Task 2**

```bash
git add src/hsm_decoder.c
git commit -m "feat: implement detailed field breakdown formatting for HSM response decoding"
```

---

### Task 3: E2E Verification & Test Suite Run

**Files:**
- Run: `tests/test_runner.py`

- [ ] **Step 1: Run complete test suite**

Run: `python tests/test_runner.py`
Expected output: All test cases PASS.

- [ ] **Step 2: Commit final changes if any**

```bash
git add .
git commit -m "test: verify HSM detailed decoding with complete test suite"
```
