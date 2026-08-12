# Project: TCP Client CLI

## Architecture
- Language & Runtime: C99 / POSIX socket APIs (lightweight, zero external dependencies, Linux standalone binary).
- Architecture Components:
  - `src/main.c` / `src/cli_args.c`/`.h`: CLI argument & flag parser (`--host`, `--port`, `--timeout`, `--interactive`, `--verbose`).
  - `src/socket_client.c`/`.h`: Socket engine with IPv4/IPv6 dual-stack resolution (`getaddrinfo`), non-blocking connection setup (`fcntl`, `connect`, `poll`, `getsockopt`), read/write timeouts, and structured exit code reporting.
  - `src/mode_interactive.c`/`.h`: Interactive terminal mode runner using POSIX `poll()` to multiplex `STDIN_FILENO` and socket real-time I/O.
  - `src/mode_oneshot.c`/`.h`: One-shot / pipe mode runner reading STDIN, streaming payload to TCP socket, `shutdown(SHUT_WR)` half-close, streaming response to STDOUT.
  - `src/signal_handler.c`/`.h`: Signal handling (`SIGINT`, `SIGPIPE` suppression with `SIG_IGN`/`MSG_NOSIGNAL`, `atexit` terminal attribute restoration).
- Build System: `Makefile` producing standalone Linux executable `./tcp-client`.
- Exit Code Matrix:
  - `0`: Success
  - `1`: Invalid Arguments / Usage Error
  - `2`: Host Resolution / DNS Failure
  - `3`: Connection Refused / Unreachable
  - `4`: Timeout Error
  - `5`: Network Socket I/O Error / Abrupt Disconnect

## Feature Inventory
Every feature from the Survey phase is enumerated below:
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

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | CLI & Core Socket Engine | Argument parsing, address resolution, socket engine, timeout, exit codes (FEAT-001..FEAT-009, FEAT-015) | none | DONE |
| M2 | Dual Operating Modes & Signal Handling | Mode auto-detection (`isatty`), Interactive Mode (`poll`), One-Shot / Pipe Mode (`shutdown`), signal handling (`SIGPIPE`/`SIGINT`) (FEAT-010..FEAT-012) | M1 | DONE |
| M3 | Build System & Packaging | Modular `Makefile`, compiler options (`-Wall -Wextra -O2`), standalone binary `./tcp-client` packaging (FEAT-013, FEAT-014) | M1, M2 | DONE |
| M4 | Final Integration & Hardening (Phase 1 & Phase 2) | E2E suite pass (Tiers 1-4) + Tier 5 adversarial coverage hardening (FEAT-016 integration) | M1, M2, M3, TEST_READY | DONE |

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
  - Returns exit code (0 on normal quit, 5 on network error).
- Function `int run_oneshot_mode(int sockfd, const cli_config_t *config)`
  - Reads STDIN, transmits to socket, sends `shutdown(sockfd, SHUT_WR)`, drains response to STDOUT, returns exit code 0 or 5.

## Code Layout
```
d:/DEV/3DS/acs_kernel_ncudcntt/tcp-client-cli/
├── src/
│   ├── main.c              # Main entry point & dispatch
│   ├── cli_args.c          # Argument & flag parsing logic
│   ├── cli_args.h
│   ├── socket_client.c     # POSIX socket lifecycle & non-blocking connect
│   ├── socket_client.h
│   ├── mode_interactive.c # Interactive poll-driven I/O loop
│   ├── mode_interactive.h
│   ├── mode_oneshot.c     # One-shot pipe processing & half-close
│   ├── mode_oneshot.h
│   ├── signal_handler.c   # Signal suppression & term cleanup
│   └── signal_handler.h
├── tests/
│   ├── mock_server.py     # Python-based multi-mode TCP mock server
│   └── test_runner.py     # Automated E2E test runner (Tiers 1-4)
├── Makefile               # Build script
└── README.md              # User documentation
```
