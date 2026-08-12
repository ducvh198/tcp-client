# TCP Client CLI — Test Infrastructure & Design Specification (`TEST_INFRA.md`)

## 1. Test Philosophy

The testing framework for **TCP Client CLI** is built around an **opaque-box (black-box), requirement-driven, end-to-end (E2E) testing model**.

- **Opaque-Box Verification**: Tests interact solely with the `./tcp-client` compiled binary via standard POSIX OS interfaces (command-line arguments, environment, standard streams STDIN/STDOUT/STDERR, socket network traffic, and process exit codes). Internal implementation details or state are not inspected.
- **Requirement-Driven**: Every test case directly traces back to functional requirements in `ORIGINAL_REQUEST.md` and feature contracts in `PROJECT.md`.
- **POSIX Standard Compliance**: Verification confirms strict adherence to POSIX CLI conventions and standard exit code behavior (0 to 5 matrix).
- **Deterministic Mocking**: All network interactions execute against a controlled, multi-mode Python TCP Mock Server (`tests/mock_server.py`), eliminating external network dependencies and ensuring zero flakiness.

---

## 2. Feature Inventory & Tier Mapping

All 16 project features (`FEAT-001` through `FEAT-016`) are mapped across the 4 Test Tiers:

| Feature ID | Feature Name | Description | Primary Tier | Test Count | Test Identifiers |
|---|---|---|---|---|---|
| `FEAT-001` | Positional Host/Port Parsing | Accepts `./tcp-client <host> <port>` syntax | Tier 1 | 4 | T1_01, T1_02, T2_03, T3_03 |
| `FEAT-002` | Flag Parameter Parsing | Supports `--host/-h`, `--port/-p` syntax | Tier 1 | 6 | T1_03, T1_04, T1_05, T2_04, T2_05, T3_01, T3_05 |
| `FEAT-003` | Timeout Flag Configuration | Supports `--timeout/-t <ms>` parameter | Tier 1 | 5 | T1_11, T1_12, T2_08, T3_03, T3_04, T3_05 |
| `FEAT-004` | Force Interactive Flag | Supports `--interactive/-i` flag | Tier 1 | 3 | T1_06, T1_07, T3_02 |
| `FEAT-005` | Verbose Logging Flag | Supports `--verbose/-v` flag | Tier 1 | 4 | T1_13, T1_14, T3_02, T3_04, T3_05 |
| `FEAT-006` | TCP Connection Handling | IPv4/IPv6 DNS lookup & connect | Tier 1 | 5 | T1_01, T1_02, T1_03, T2_06, T2_07 |
| `FEAT-007` | Timeout Detection | Connect/read/write timeout enforcement | Tier 1 | 4 | T1_11, T1_12, T1_20, T2_08, T3_03 |
| `FEAT-008` | Disconnect & EOF Detection | Detects clean EOF and abrupt server disconnects | Tier 1 | 3 | T1_21, T4_04 |
| `FEAT-009` | Structured Error Reporting | Exit codes 0-5 and STDERR error strings | Tier 1 | 8 | T1_16, T1_17, T1_18, T1_19, T1_20, T1_21, T2_06, T2_07 |
| `FEAT-010` | Auto-Mode Detection | Auto-select mode via `isatty(STDIN)` | Tier 1 | 2 | T1_15, T1_08 |
| `FEAT-011` | Interactive Mode Terminal | Prompt `> `, real-time exchange, `exit`/`quit` | Tier 1 & 4 | 4 | T1_06, T1_07, T3_02, T4_03 |
| `FEAT-012` | One-Shot / Pipe Mode | Pipe input streaming, `shutdown(SHUT_WR)`, STDOUT | Tier 1, 2, 4 | 8 | T1_08, T1_09, T1_10, T1_15, T2_01, T2_02, T3_01, T4_01, T4_02 |
| `FEAT-013` | Standalone Linux Binary | Single standalone C executable | Tier 1-4 | All | Exercised across all test executions |
| `FEAT-014` | Automated Build Script | `Makefile` building `./tcp-client` | Runner | Infra | Runner handles missing binary gracefully |
| `FEAT-015` | Standard Exit Code Matrix | Exit codes 0 to 5 matching specs | Tier 1-4 | 9 | T1_16..T1_21, T2_04..T2_08, T4_04 |
| `FEAT-016` | Automated Test Suite | Python test runner & mock TCP server | Infra | Infra | Entire `tests/` directory suite |

---

## 3. Test Architecture

The test suite consists of two core components written strictly using Python 3 standard library modules:

```
tests/
├── mock_server.py     # Configurable TCP mock server (echo, delayed, disconnect, multiline, http)
└── test_runner.py     # E2E test runner executing 4-tier test cases against binary & mock server
```

### A. Mock Server Architecture (`tests/mock_server.py`)
- **Dynamic Free Port Allocation**: When `--port 0` is passed, the server binds to port 0, gets the allocated kernel port via `getsockname()`, and outputs `PORT: <port>` to STDOUT so the test runner can dynamically parse it.
- **Operating Modes**:
  1. `echo`: Reflects all received bytes directly back to client until EOF.
  2. `delayed`: Delays response by `--delay-ms` to test client timeout detection (Exit code 4).
  3. `disconnect`: Closes connection immediately or after `--disconnect-after` N bytes to test abrupt disconnect handling (Exit code 5).
  4. `multiline`: Simulates interactive terminal exchanges, responding with `ACK: <line>` and handling `exit`/`quit`.
  5. `http`: Returns a structured `HTTP/1.1 200 OK` response with headers and body (`Hello, World!`).
- **CLI Flags**: `--host`, `--port`, `--mode`, `--delay-ms`, `--disconnect-after`, `--quiet`, `--port-file`, `--once`.

### B. Automated Test Runner Architecture (`tests/test_runner.py`)
- **Process Isolation**: Spawns `./tcp-client` in a dedicated subprocess for every test case.
- **Standard I/O Redirection**: Feeds custom binary/text byte streams into binary STDIN and captures STDOUT/STDERR separately.
- **Exit Code Verification**: Validates expected exit code against the project matrix:
  - `0`: Success
  - `1`: Usage / Argument Error
  - `2`: DNS / Host Resolution Failure
  - `3`: Connection Refused
  - `4`: Timeout Error
  - `5`: Network Socket I/O / Disconnect Error
- **CLI Options**:
  - `--binary <path>`: Custom path to `tcp-client` binary.
  - `--tier <1|2|3|4|all>`: Selective execution by Tier.
  - `--verbose`: Detailed test logging.
  - `--list`: Inventory listing without binary invocation.
- **Missing Binary Graceful Degradation**: If `./tcp-client` binary is not found, `--list` and infrastructure validation still succeed cleanly with informative warnings.

---

## 4. Real-World Application Scenarios (Tier 4)

Tier 4 validates end-to-end integration across realistic production use cases:

1. **`T4_01` HTTP GET Simulation**:
   Pipes an HTTP GET request string (`GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`) to `tcp-client` connected to an HTTP mock server. Verifies that HTTP response headers and body are written cleanly to STDOUT and exit code is 0.
2. **`T4_02` JSON File Pipe Transmission**:
   Pipes a formatted JSON payload through `tcp-client` into an echo server. Verifies exact structural JSON preservation on STDOUT.
3. **`T4_03` Multi-Message Interactive Session**:
   Simulates an interactive session by feeding multiple command lines (`PING`, `PONG`, `exit`) into interactive mode (`-i`). Verifies line-by-line responses and graceful session termination.
4. **`T4_04` Premature Server Disconnect Mid-Transfer**:
   Transmits a multi-byte stream to a mock server configured to drop connection after 10 bytes. Verifies `tcp-client` detects socket error, writes error message to STDERR, and exits with code 5.

---

## 5. Coverage Thresholds & Quality Criteria

The test suite enforces the following quantitative quality thresholds:

- **Tier 1 (Feature Coverage)**: Requires $\ge 5$ test cases per feature category. Total Tier 1 test cases: **21**.
- **Tier 2 (Boundary & Corner Cases)**: Requires $\ge 5$ boundary cases covering empty payload, large payload (>1MB streaming), port boundaries (65535, >65535, non-numeric), invalid IP, closed port, and instant timeout (1ms). Total Tier 2 test cases: **8**.
- **Tier 3 (Cross-Feature Combinations)**: Requires pairwise and full flag combination test cases (flags + pipe, `-i` + `-v`, positional + `-t`, `-t` + `-v`, all flags combined). Total Tier 3 test cases: **5**.
- **Tier 4 (Real-World Application Scenarios)**: Requires real-world protocols and integration workloads (HTTP, JSON pipe, interactive session, mid-transfer disconnect). Total Tier 4 test cases: **4**.
- **Total Test Count**: **38 test cases** across Tiers 1-4.
