# TCP Client CLI (`tcp-client`)

A lightweight, high-performance, standalone C99/POSIX command-line TCP client utility for Unix-like operating systems. Designed for network debugging, service integration, pipe payload transmission, and real-time interactive TCP sessions with zero external runtime dependencies.

---

## Features

- **Strict POSIX C99 Standard**: Built using standard C99 and POSIX socket APIs (`sys/socket.h`, `netdb.h`, `poll.h`, `fcntl.h`). Zero third-party runtime libraries required beyond standard `libc`.
- **Dual-Stack IPv4 / IPv6 Support**: Automatic host name and IP address resolution using POSIX `getaddrinfo()`.
- **Dual Operating Modes**:
  - **One-Shot / Pipe Mode**: Streams input payload from `STDIN` to the TCP socket, performs socket half-close (`shutdown(SHUT_WR)`), and streams response back to `STDOUT`. Ideal for script piping and automation.
  - **Interactive Terminal Mode**: Multiplexes keyboard input (`STDIN`) and socket real-time I/O using `poll()`, featuring terminal prompt (`> `), line buffering, and commands (`exit`/`quit`).
  - **Automatic Mode Detection**: Seamlessly detects whether `STDIN` is connected to an interactive TTY (`isatty()`) or a pipe, selecting the appropriate mode automatically unless explicitly overridden via `--interactive`.
- **Non-Blocking Socket Engine**: Uses non-blocking socket configuration (`O_NONBLOCK`), explicit connection timeout handling, and read/write timeout control.
- **Robust Signal Handling**: Suppresses `SIGPIPE` on broken connection writes (`MSG_NOSIGNAL` / `SIG_IGN`), restoring terminal attributes on exit via `atexit()`.
- **Structured Exit Code Matrix**: Standardized exit codes (0 to 5) enabling deterministic error handling in automated shell scripts and CI/CD pipelines.

---

## Build & Installation

### Prerequisites
- POSIX-compliant Unix-like operating system (Linux, macOS, BSD).
- C compiler (`gcc` or `clang`) supporting C99.
- GNU `make` build utility.
- `python3` (required for running the automated E2E test suite).

### Building from Source

To compile the standalone binary `./tcp-client` in the project root directory:

```bash
make
```

### Running Tests

To run the automated E2E test suite (38 test cases across unit, mode, error, and timeout scenarios):

```bash
make test
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
| `-a` | `--ascii <str>` | Directly send raw ASCII payload string (e.g. `"NC0000"`) | N/A |
| `-x` | `--hex <hex_str>`| Directly send raw HEX payload string (e.g. `"00 06 30 30 30 30"`) | N/A |
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

1. **Start Interactive TCP Session**:
   ```bash
   ./tcp-client 127.0.0.1 9000
   ```
   *Terminal Output*:
   ```text
   Connected to 127.0.0.1:9000
   > PING
   PONG
   > HELP
   UNKNOWN COMMAND
   > exit
   Connection closed.
   ```

2. **Force Interactive Mode when Piping**:
   ```bash
   ./tcp-client --host localhost --port 8080 --interactive --timeout 10000
   ```

---

## Exit Code Matrix

The `./tcp-client` application returns standardized exit codes for scriptability and error verification:

| Exit Code | Macro Symbol | Description / Trigger Condition |
|:---:|---|---|
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
│   ├── mode_interactive.c # Interactive mode runner using poll() STDIN multiplexing
│   ├── mode_interactive.h # run_interactive_mode() function interface
│   ├── mode_oneshot.c     # One-shot pipe mode runner & shutdown(SHUT_WR)
│   ├── mode_oneshot.h     # run_oneshot_mode() function interface
│   ├── signal_handler.c   # SIGPIPE/SIGINT handlers & terminal restoration
│   └── signal_handler.h   # Signal handling interface declarations
├── tests/
│   ├── mock_server.py     # Python multi-mode TCP test server (echo/delayed/disconnect)
│   └── test_runner.py     # E2E test suite runner executing 38 automated tests
├── Makefile               # Build script with auto-dependency tracking (-MMD -MP)
└── README.md              # Project documentation & user guide
```
