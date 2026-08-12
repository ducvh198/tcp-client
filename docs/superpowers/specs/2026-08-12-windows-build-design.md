# Design Spec: Windows Target Build & Cross-Platform Support

## Executive Summary
This design specification details the architecture and implementation for cross-compiling and building a native, zero-dependency Windows executable (`tcp-client.exe`) alongside the existing Linux binary (`tcp-client`).

## Goals & Constraints
- **Native Windows Binary**: Produce a standalone `tcp-client.exe` binary with no external runtime DLL dependencies (uses standard Windows Winsock `ws2_32.lib`).
- **Single Modular Codebase**: Retain a unified C99 codebase using a lightweight platform abstraction header (`src/compat.h`).
- **Feature Parity**: Full support on Windows for dual modes (Interactive terminal mode, One-shot/Pipe mode), custom timeouts, HEX payloads, HSM response decoding, and standard exit codes (0 to 5).
- **Dual Target Build System**: Update `Makefile` to support building Linux target (`make`) and Windows cross-compilation (`make win` / `make TARGET=tcp-client.exe`).

---

## Component Design & Platform Abstraction

### 1. Platform Abstraction Header (`src/compat.h`)
A new abstraction header will unify POSIX and Windows Winsock network and system calls:

```c
#ifndef COMPAT_H
#define COMPAT_H

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <io.h>
  
  #define socket_close(s) closesocket(s)
  #define SHUT_WR SD_SEND
  #define STDIN_FILENO 0
  #define STDOUT_FILENO 1
  #define STDERR_FILENO 2

  /* Winsock Initialization & Cleanup */
  int platform_init(void);
  void platform_cleanup(void);
  int set_socket_nonblocking(int sockfd);
  int get_last_socket_error(void);
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <poll.h>
  #include <errno.h>

  #define socket_close(s) close(s)
  #define platform_init() (0)
  #define platform_cleanup() ((void)0)
  int set_socket_nonblocking(int sockfd);
  int get_last_socket_error(void);
#endif

#endif /* COMPAT_H */
```

### 2. Socket Client Engine (`src/socket_client.c`)
- Calls `platform_init()` / `platform_cleanup()`.
- Wraps `fcntl(O_NONBLOCK)` vs `ioctlsocket(FIONBIO)` via `set_socket_nonblocking()`.
- Maps Windows Winsock error codes (`WSAECONNREFUSED`, `WSAEHOSTUNREACH`, `WSAETIMEDOUT`, `WSAEWOULDBLOCK`) alongside POSIX `errno` to standard return codes (`SOCKET_ERR_REFUSED`, `SOCKET_ERR_TIMEOUT`, `SOCKET_ERR_IO`).
- Handles `close()` vs `closesocket()`.

### 3. Signal & Terminal Handler (`src/signal_handler.c`)
- On POSIX: Uses `<termios.h>` and `sigaction(SIGINT)` / `signal(SIGPIPE, SIG_IGN)`.
- On Windows (`_WIN32`): Uses `GetConsoleMode()` and `SetConsoleMode()` to manage console input attributes, and `signal(SIGINT, handle_signal)` for interrupt handling.

### 4. Interactive & One-Shot Mode Multiplexing (`src/mode_interactive.c`, `src/mode_oneshot.c`)
- On POSIX: `poll()` on `STDIN_FILENO` and `sockfd`.
- On Windows: Uses `WSAPoll` for sockets, and unified non-blocking checks / standard I/O calls for STDIN.

---

## Build System Integration (`Makefile`)

The `Makefile` is enhanced to support both native Linux builds and cross-compilation for Windows using `x86_64-w64-mingw32-gcc`:

```make
# Default target settings (Linux)
CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -pedantic -std=c99 -O2
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
LIBS =

# Windows Cross-Compilation Target: `make win`
win:
	$(MAKE) CC=x86_64-w64-mingw32-gcc TARGET=tcp-client.exe LIBS="-lws2_32" CPPFLAGS=""
```

---

## Verification & Testing Plan
1. **Linux Build & Test Suite**: Run `make clean && make test` inside WSL to verify no regression on Linux.
2. **Windows Cross-Build**: Run `make win` to compile `tcp-client.exe`.
3. **Windows Binary Verification**: Test `tcp-client.exe` execution under Windows PowerShell (pos/flag args, timeout, interactive, hex mode, exit codes 0-5).
