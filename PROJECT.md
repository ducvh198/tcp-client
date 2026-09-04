# Project: TCP Client CLI

## Architecture
- Language & Runtime: C99 / POSIX socket APIs (lightweight, zero external dependencies, standalone binary for Linux and Windows MinGW).
- Architecture Components:
  - `src/main.c` / `src/cli_args.c`/`.h`: CLI argument & flag parser (`--host`, `--port`, `--timeout`, `--interactive`, `--verbose`, `-x`/`--hex`, `-a`/`--ascii`, `-T`/`--term`, `-X`/`--hex-out`, `-L`/`--add-tcp-len`, `-D`/`--decode-hsm`).
  - `src/socket_client.c`/`.h`: Socket engine with IPv4/IPv6 dual-stack resolution (`getaddrinfo`), non-blocking connection setup (`connect`, `poll`/`WSAPoll`), read/write timeouts, and structured exit code reporting.
  - `src/linenoise.c`/`.h`: Embedded zero-dependency cross-platform line-editing engine providing raw terminal mode, Up/Down arrow history navigation, Tab auto-completion, and incremental line feed/redraw.
  - `src/mode_interactive.c`/`.h`: Rich interactive terminal mode runner multiplexing keyboard and socket I/O at 20ms slices, built-in commands (`help`, `clear`, `status`, `history`, `exit`, `quit`), persistent history (`~/.tcp_client_history` / `%USERPROFILE%\.tcp_client_history`, max 500 lines), clean terminal restoration, memory hygiene (`linenoiseHistoryFree`), and seamless TTY/pipe fallback.
  - `src/mode_oneshot.c`/`.h`: One-shot / pipe mode runner reading STDIN, streaming payload to TCP socket, `shutdown(SHUT_WR)` half-close, streaming response to STDOUT.
  - `src/hex_utils.c`/`.h`: HEX parsing, formatting, validation, and binary conversion utilities.
  - `src/hsm_decoder.c`/`.h`: payShield 10K HSM response decoder & analysis formatter.
  - `src/compat.c`/`.h`: Windows (Winsock2, WSAPoll, `_kbhit`, `_getch`) and POSIX abstraction layer.
  - `src/signal_handler.c`/`.h`: Signal handling (`SIGINT`, `SIGPIPE` suppression with `SIG_IGN`/`MSG_NOSIGNAL`, `atexit` terminal attribute restoration).
- Build System: `Makefile` producing standalone Linux executable `./tcp-client` and Windows MinGW binary `tcp-client.exe`.
- Exit Code Matrix:
  - `0`: Success
  - `1`: Invalid Arguments / Usage Error
  - `2`: Host Resolution / DNS Failure
  - `3`: Connection Refused / Unreachable
  - `4`: Timeout Error
  - `5`: Network Socket I/O Error / Abrupt Disconnect

## Feature Inventory
Every feature from the Survey and Enhancement phases is enumerated below:
| # | Feature ID | Feature Name | Description | Milestone | Source |
|---|---|---|---|---|---|
| 1 | `FEAT-001` | Positional Host/Port Parsing | Accepts `./tcp-client <host> <port>` syntax | M1 | ORIGINAL_REQUEST.md R1 |
| 2 | `FEAT-002` | Flag Parameter Parsing | Supports `--host/-h`, `--port/-p` syntax | M1 | ORIGINAL_REQUEST.md R1 |
| 3 | `FEAT-003` | Timeout Flag Configuration | Supports `--timeout/-t <ms>` parameter | M1 | Implicit / Task |
| 4 | `FEAT-004` | Force Interactive Flag | Supports `--interactive/-i` flag | M1 | Implicit / Task |
| 5 | `FEAT-005` | Verbose Logging Flag | Supports `--verbose/-v` flag | M1 | Implicit / Task |
| 6 | `FEAT-006` | TCP Connection Handling | IPv4/IPv6 DNS lookup & connect | M1 | ORIGINAL_REQUEST.md R1 |
| 7 | `FEAT-007` | Timeout Detection | Connect/read/write timeout enforcement | M1 | ORIGINAL_REQUEST.md R1 |
| 8 | `FEAT-008` | Disconnect & EOF Detection | Detects clean EOF and server disconnects | M1 | ORIGINAL_REQUEST.md R1 |
| 9 | `FEAT-009` | Structured Error Reporting | Exit codes 0-5 and STDERR error strings | M1 | ORIGINAL_REQUEST.md R1 |
| 10 | `FEAT-010` | Auto-Mode Detection | Auto-select mode via `isatty(STDIN)` | M2 | ORIGINAL_REQUEST.md R2 |
| 11 | `FEAT-011` | Interactive Mode Terminal | Prompt `> `, real-time exchange, `exit`/`quit` | M2 | ORIGINAL_REQUEST.md R2 |
| 12 | `FEAT-012` | One-Shot / Pipe Mode | Pipe input streaming, `shutdown(SHUT_WR)`, STDOUT | M2 | ORIGINAL_REQUEST.md R2 |
| 13 | `FEAT-013` | Standalone Linux Binary | Single standalone C executable | M3 | ORIGINAL_REQUEST.md R3 |
| 14 | `FEAT-014` | Automated Build Script | `Makefile` building `./tcp-client` | M3 | Acceptance Criteria |
| 15 | `FEAT-015` | Standard Exit Code Matrix | Exit codes 0 to 5 matching specs | M1 | Acceptance Criteria |
| 16 | `FEAT-016` | Automated Test Suite | Python test runner & mock TCP server | E2E-Track | Acceptance Criteria |
| 17 | `FEAT-017` | Linenoise Line Editing & History | Up/Down arrow history, cursor navigation | M5 | Linenoise SDD Task 1 |
| 18 | `FEAT-018` | Tab Auto-Completion | Auto-completes built-in & payment commands | M5 | Linenoise SDD Task 2 |
| 19 | `FEAT-019` | Persistent Command History | Persists up to 500 commands to user dotfile | M5 | Linenoise SDD Task 2 |
| 20 | `FEAT-020` | Real-Time Multiplexing & Redraw | Non-blocking socket I/O with seamless prompt redraw | M5 | Linenoise SDD Task 3 |
| 21 | `FEAT-021` | Built-in Interactive Commands | `help`, `clear`, `status`, `history`, `exit`, `quit` | M5 | Linenoise SDD Task 3 |
| 22 | `FEAT-022` | TTY Auto-Detection & Fallback | Graceful fallback to non-blocking pipe reader | M5 | Linenoise SDD Task 3 |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | CLI & Core Socket Engine | Argument parsing, address resolution, socket engine, timeout, exit codes (FEAT-001..FEAT-009, FEAT-015) | none | DONE |
| M2 | Dual Operating Modes & Signal Handling | Mode auto-detection (`isatty`), Interactive Mode (`poll`), One-Shot / Pipe Mode (`shutdown`), signal handling (`SIGPIPE`/`SIGINT`) (FEAT-010..FEAT-012) | M1 | DONE |
| M3 | Build System & Packaging | Modular `Makefile`, compiler options (`-Wall -Wextra -O2`), standalone binary `./tcp-client` packaging (FEAT-013, FEAT-014) | M1, M2 | DONE |
| M4 | Final Integration & Hardening (Phase 1 & Phase 2) | E2E suite pass (Tiers 1-4) + Tier 5 adversarial coverage hardening (FEAT-016 integration) | M1, M2, M3, TEST_READY | DONE |
| M5 | Linenoise Interactive Terminal Integration | Embedded cross-platform Linenoise line-editing engine, Tab auto-completion, persistent history, real-time 20ms socket multiplexing with asynchronous prompt redraw, built-in commands (`help`, `clear`, `status`, `history`), and clean memory hygiene (`linenoiseHistoryFree`) (FEAT-017..FEAT-022) | M1..M4 | DONE |

## Interface Contracts
### `cli_args` -> `socket_client` / `mode_runner`
- Struct `cli_config_t`:
  ```c
  typedef struct {
      char host[256];
      int port;
      int timeout_ms;
      bool force_interactive;
      bool verbose;
      client_mode_t mode; // MODE_AUTO, MODE_INTERACTIVE, MODE_ONESHOT
      char raw_hex[MAX_HEX_STR_LEN];
      bool has_hex;
      char raw_ascii[MAX_ASCII_STR_LEN];
      bool has_ascii;
      bool add_tcp_len;
      bool hex_out;
      bool decode_hsm;
      int hsm_header_len;
      bool has_term_char;
      uint8_t term_char;
  } cli_config_t;
  ```
- Function `int parse_cli_args(int argc, char *argv[], cli_config_t *config)`
  - Returns `0` on success, `1` on usage/parsing error.

### `socket_client` API
- Function `int socket_connect(const char *host, int port, int timeout_ms, bool verbose)`
  - Returns non-negative `sockfd` on success, or negative exit code (`-2` DNS, `-3` Refused, `-4` Timeout, `-5` I/O Error) on failure.
- Function `void socket_close(int sockfd)`

### `mode_runner` API
- Function `int run_interactive_mode(int sockfd, const cli_config_t *config)`
  - Gates on TTY state; delegates to `run_interactive_tty_mode` (with Linenoise line-editing, history, Tab completion, and redraw) or `run_interactive_pipe_mode` (non-blocking stream reader).
  - Returns exit code (0 on normal quit, 5 on network error).
- Function `int run_oneshot_mode(int sockfd, const cli_config_t *config)`
  - Reads STDIN or sends payload buffer, transmits to socket, sends `shutdown(sockfd, SHUT_WR)`, drains response to STDOUT, returns exit code 0 or 5.

## Code Layout
```
d:/DEV/3DS/acs_kernel_ncudcntt/tcp-client-cli/
├── src/
│   ├── main.c              # Main entry point & dispatch
│   ├── cli_args.c          # Argument & flag parsing logic
│   ├── cli_args.h          # Config struct cli_config_t & CLI declarations
│   ├── socket_client.c     # POSIX socket lifecycle & non-blocking connect
│   ├── socket_client.h     # Socket API definitions & SOCKET_ERR error codes
│   ├── mode_interactive.c # Rich Linenoise interactive terminal & TTY fallback
│   ├── mode_interactive.h # run_interactive_mode() & completion callback declarations
│   ├── mode_oneshot.c     # One-shot pipe processing & half-close
│   ├── mode_oneshot.h     # run_oneshot_mode() function interface
│   ├── signal_handler.c   # Signal suppression & term cleanup
│   ├── signal_handler.h   # Signal handling interface declarations
│   ├── hex_utils.c        # HEX parser, validator & formatters
│   ├── hex_utils.h        # HEX utility declarations
│   ├── hsm_decoder.c      # payShield 10K HSM Response Decoder
│   ├── hsm_decoder.h      # HSM decoder data models and analysis printer
│   ├── compat.c           # Windows/POSIX compatibility layer (poll_sockets)
│   ├── compat.h           # Cross-platform socket and poll abstraction macros
│   ├── linenoise.c        # Embedded Linenoise line-editing engine with raw mode
│   └── linenoise.h        # Linenoise public API definitions
├── tests/
│   ├── mock_server.py     # Python-based multi-mode TCP mock server
│   ├── test_runner.py     # Automated E2E test runner (65 tests across Tiers 1-5)
│   ├── test_adversarial.py# Tier 5 adversarial edge case test suite (14 tests)
│   ├── stress_cli_parser.py# Empirical CLI argument parser stress tests (34 tests)
│   ├── test_interactive_logic.py # Interactive completion & history unit test runner
│   └── test_linenoise_c.c # Standalone C unit tests for Linenoise core functions
├── Makefile               # Build script
└── README.md              # User documentation
```
