# TCP Client CLI (`tcp-client`)

A lightweight, high-performance, standalone C99/POSIX command-line TCP client utility for Unix-like operating systems. Designed for network debugging, service integration, pipe payload transmission, and real-time interactive TCP sessions with zero external runtime dependencies.

---

## Features

- **Strict POSIX C99 Standard**: Built using standard C99 and POSIX socket APIs (`sys/socket.h`, `netdb.h`, `poll.h`, `fcntl.h`). Zero third-party runtime libraries required beyond standard `libc`.
- **Dual-Stack IPv4 / IPv6 Support**: Automatic host name and IP address resolution using POSIX `getaddrinfo()`.
- **Dual Operating Modes**:
  - **One-Shot / Pipe Mode**: Streams input payload from `STDIN` to the TCP socket, performs socket half-close (`shutdown(SHUT_WR)`), and streams response back to `STDOUT`. Ideal for script piping and automation.
  - **Enhanced Interactive Terminal Mode**: Powered by an embedded cross-platform `linenoise` line-editing engine with 20ms real-time socket multiplexing:
    - **Command History Navigation**: Up / Down arrow keys seamlessly scroll through previously entered commands.
    - **Tab Auto-Completion**: Context-aware completion for built-in control commands (`exit`, `quit`, `help`, `clear`, `status`, `history`) and common HSM/payment sample commands (`NC0000`, `00 06 30 30 30 30`, `BA`, `BB`, `CA`, `CB`, `M0`, `M2`).
    - **Persistent Command History**: Automatically loads and flushes command history to `~/.tcp_client_history` (POSIX) or `%USERPROFILE%\.tcp_client_history` (Windows), retaining up to 500 recent entries across sessions.
    - **Real-Time Multiplexing & Seamless Redraw**: Incoming server packets asynchronously clear the current line (`\r\x1b[2K`), print server messages, and restore the prompt and pending edit buffer without cursor corruption or ghosting.
    - **Built-in Helper Commands**: Native local execution of `help` (command list & shortcuts), `clear` (screen wipe), `status` (connection details & FD state), `history` (numbered history log), and `exit`/`quit`.
  - **Automatic Mode Detection & TTY Fallback**: Automatically detects whether `STDIN` is connected to an interactive TTY (`isatty()`) or a redirected pipe. When redirected, falls back to a high-throughput non-blocking stream line reader, preserving full script automation compatibility.
- **Non-Blocking Socket Engine**: Uses non-blocking socket configuration (`O_NONBLOCK`), explicit connection timeout handling, and read/write timeout control.
- **Robust Signal Handling**: Suppresses `SIGPIPE` on broken connection writes (`MSG_NOSIGNAL` / `SIG_IGN`), restoring terminal attributes on exit via `atexit()`.
- **Structured Exit Code Matrix**: Standardized exit codes (0 to 5) enabling deterministic error handling in automated shell scripts and CI/CD pipelines.

---

## Build & Installation

### Prerequisites
- POSIX-compliant Unix-like operating system (Linux, macOS, BSD) or Windows (MinGW).
- C compiler (`gcc` or `clang`) supporting C99.
- GNU `make` build utility.
- `python3` (required for running the automated E2E test suite).

### Building from Source

To compile the standalone binary `./tcp-client` in the project root directory:

```bash
make
```

For Windows MinGW compilation producing `tcp-client.exe`:

```bash
make win
```

### Running Tests

To run the full automated test suites (over 115 tests across E2E, adversarial, stress, and interactive logic):

```bash
# Automated E2E test suite (65 test cases)
python tests/test_runner.py

# Adversarial edge case suite (14 test cases)
python tests/test_adversarial.py

# CLI parser stress suite (34 test cases)
python tests/stress_cli_parser.py

# Interactive logic unit tests (4 test cases)
python tests/test_interactive_logic.py
```

### Cleaning Build Artifacts

To remove compiled object files, dependency tracking files (`build/`), and the `./tcp-client` binary:

```bash
make clean
```

### Installation & Uninstallation

To install the binary to `/usr/local/bin` (or a custom `PREFIX` / `DESTDIR`):

```bash
# Standard installation to /usr/local/bin
sudo make install

# Custom prefix installation (e.g. ~/.local/bin)
make install PREFIX=$HOME/.local

# Uninstalling the binary
sudo make uninstall
```

---

## CLI Usage & Options

### Syntax

```bash
# Positional syntax
./tcp-client <host> <port> [options]

# Flag-based syntax
./tcp-client --host <host> --port <port> [options]
```

### Command Line Options Table

| Short Flag | Long Flag | Description | Default / Details |
|---|---|---|---|
| `-h` | `--host <host>` | Target hostname or IP address (IPv4/IPv6) | Required |
| `-p` | `--port <port>` | Target TCP port number (1 to 65535) | Required |
| `-t` | `--timeout <ms>` | Connection and read/write timeout in milliseconds | `5000` (5 seconds) |
| `-a` | `--ascii <str>` | Directly send raw ASCII payload string with escape sequence support (`\xHH`, `\r`, `\n`, `\t`, `\0`, `\\`) | N/A |
| `-x` | `--hex <hex_str>`| Directly send raw HEX payload string (e.g. `"00 06 30 30 30 30"`) | N/A |
| `-T` | `--term <hex>` | 1-byte HEX termination character: appends on send and stops reading on receive delimiter (e.g. `"19"` or `"0x19"`) | Disabled |
| `-X` | `--hex-out` | Format server response as HEX string on `STDOUT` | Disabled |
| `-L` | `--add-tcp-len` | Prepend 2-byte Big-Endian TCP length header to ASCII/HEX payload | Disabled |
| `-D` | `--decode-hsm` | Enable payShield 10K HSM Response Decoder analysis report | Disabled |
| | `--hsm-header-len <n>` | Set HSM Message Header length in bytes | `0` (or `4` for `HDR1`) |
| `-i` | `--interactive` | Force Interactive Mode (overrides pipe auto-detection) | Auto-detected via `isatty()` |
| `-v` | `--verbose` | Enable diagnostic and status logging to `STDERR` | Disabled |
| `-H` | `--help` | Display usage instructions and exit | N/A |
| `-V` | `--version` | Display application version information and exit | N/A |

---

## Usage Examples

### HSM Host Command Testing & Response Decoding (payShield 10K)

1. **Send HSM Network Check Command & Decode Response**:
   ```bash
   ./tcp-client 127.0.0.1 8000 -x "00 06 30 30 30 30" -D
   ```
   *Decoder Output*:
   ```text
   ======================================================================
                payShield 10K HSM RESPONSE DECODER ANALYSIS              
   ======================================================================
   Raw Packet Length : 10 bytes
   Raw Hex Packet    : 00 06 4E 44 30 30 30 30 30 30
   ----------------------------------------------------------------------
   TCP Length Header : 2 Bytes (Binary Big-Endian)
   Message Header    : (None)
   Response Code     : 'ND'
   Error Code        : '00' -> [SUCCESS / OK]
                       EN: No error
                       VI: Thành công hoàn toàn (Không có lỗi)
   ----------------------------------------------------------------------
   Response Payload  : 2 bytes 
   Payload HEX       : 30 30
   Payload ASCII     : 00
   ======================================================================
   ```

2. **Send HSM Command with 4-Byte Message Header (`HDR1`) & Analyze Response**:
   ```bash
   ./tcp-client --host 192.168.1.10 --port 1500 -x "00 0A 48 44 52 31 4E 44 30 30 30 30" -D --hsm-header-len 4
   ```

3. **Send HSM Key Generation Command & View Raw HEX Output**:
   ```bash
   ./tcp-client 10.0.0.50 9999 -x "00 0E 41 41 30 30 30 30 31 32 33 34 35 36 37 38" --hex-out -v
   ```

### ASCII Mode & Special Characters / Termination

1. **Send ASCII Payload with Hex Escape Sequence (`\x19`, `\r\n`)**:
   ```bash
   ./tcp-client 127.0.0.1 8000 -a "NC0000\x19"
   ```

2. **Send ASCII Payload and Append Termination Character (`--term 19`)**:
   ```bash
   ./tcp-client 127.0.0.1 8000 -a "NC0000" --term 19
   ```

3. **Stop Receiving Immediately upon Delimiter (`--term 19`)**:
   ```bash
   # Client sends request and stops reading response as soon as 0x19 is received:
   ./tcp-client 127.0.0.1 8000 -a "GET_DATA" -T 0x19
   ```

### One-Shot / Pipe Mode Examples

1. **Echo Payload to TCP Service**:
   ```bash
   echo "Hello TCP Server" | ./tcp-client 127.0.0.1 8080
   ```

2. **HTTP GET Request**:
   ```bash
   printf "GET / HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n" | ./tcp-client httpbin.org 80
   ```

3. **Binary File Streaming**:
   ```bash
   ./tcp-client --host 192.168.1.100 --port 9000 < input.dat > response.dat
   ```

4. **Verbose Pipeline Debugging with Custom Timeout**:
   ```bash
   echo "TEST_DATA" | ./tcp-client --host 127.0.0.1 --port 5000 --timeout 2000 --verbose
   ```

### Interactive Mode Examples

1. **Start Rich Interactive TCP Session**:
   ```bash
   ./tcp-client 127.0.0.1 9000
   ```
   *Terminal Output*:
   ```text
   Connected to 127.0.0.1:9000
   > help
   Interactive Terminal Commands:
     help     - Display this command reference
     clear    - Clear terminal screen
     status   - Show connection parameters and socket state
     history  - Show recent command history
     exit     - Disconnect and exit session
     quit     - Disconnect and exit session
   Key Bindings:
     Up/Down  - Navigate through command history
     Tab      - Auto-complete commands and sample payloads
     Ctrl+C   - Clear current line, or exit if line is empty
     Ctrl+D   - Exit session (when line is empty)
   > status
   Connection Status:
     Target Host : 127.0.0.1
     Target Port : 9000
     Timeout     : 5000 ms
     Socket FD   : 3
     Mode        : Interactive (TTY Linenoise)
   > NC0000
   ND00
   > history
     1  help
     2  status
     3  NC0000
   > exit
   [VERBOSE] User requested exit.
   ```

2. **Force Interactive Mode when Piping**:
   ```bash
   ./tcp-client --host localhost --port 8080 --interactive --timeout 10000
   ```

---

## Exit Code Matrix

The `./tcp-client` application returns standardized exit codes for scriptability and error verification:

| Exit Code | Macro Symbol | Description / Trigger Condition |
:---:|---|---|
| **`0`** | `EXIT_SUCCESS` | Operation completed successfully (connection closed cleanly or payload transferred). |
| **`1`** | `EXIT_USAGE` | Invalid command line arguments, missing required host/port, invalid port range, or unknown option. |
| **`2`** | `EXIT_DNS` | Host resolution failure or DNS lookup error (`getaddrinfo` returned non-zero). |
| **`3`** | `EXIT_REFUSED` | TCP connection refused by remote host, network unreachable, or host unreachable. |
| **`4`** | `EXIT_TIMEOUT` | Connection attempt, socket read operation, or write operation timed out before completion. |
| **`5`** | `EXIT_IO_ERROR` | Network socket I/O error, read failure, write error, or unexpected disconnect. |

---

## Architecture & Directory Layout

```
tcp-client-cli/
├── src/
│   ├── main.c              # Main entry point, argument validation & mode dispatch
│   ├── cli_args.c          # Flag & positional command-line argument parser
│   ├── cli_args.h          # Config struct cli_config_t & CLI declarations
│   ├── socket_client.c     # Non-blocking IPv4/IPv6 socket setup, connect & I/O
│   ├── socket_client.h     # Socket API definitions & SOCKET_ERR error codes
│   ├── mode_interactive.c # Rich Linenoise interactive terminal & TTY fallback
│   ├── mode_interactive.h # run_interactive_mode() & completion callback declarations
│   ├── mode_oneshot.c     # One-shot pipe mode runner & shutdown(SHUT_WR)
│   ├── mode_oneshot.h     # run_oneshot_mode() function interface
│   ├── signal_handler.c   # SIGPIPE/SIGINT handlers & terminal restoration
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
│   ├── mock_server.py     # Python multi-mode TCP test server (echo/delayed/disconnect)
│   ├── test_runner.py     # E2E test suite runner executing 65 automated tests
│   ├── test_adversarial.py# Adversarial edge-case test suite (14 tests)
│   ├── stress_cli_parser.py# Empirical CLI argument parser stress test suite (34 tests)
│   ├── test_interactive_logic.py # Interactive completion & history unit test runner
│   └── test_linenoise_c.c # Standalone C unit tests for Linenoise core functions
├── Makefile               # Build script with auto-dependency tracking (-MMD -MP)
└── README.md              # Project documentation & user guide
```
