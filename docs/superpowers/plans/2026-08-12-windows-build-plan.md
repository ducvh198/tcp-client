# Windows Target Build & Cross-Platform Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a native Windows executable `tcp-client.exe` using `x86_64-w64-mingw32-gcc` and Winsock2 (`ws2_32.lib`), while preserving 100% Linux target functionality (`./tcp-client`).

**Architecture:** Create a platform compatibility header (`src/compat.h`) and implementation (`src/compat.c`) to abstract socket operations, non-blocking socket setup, error code translation, and console signal handling between POSIX and Windows. Update Makefile to support cross-compiling with `make win`.

**Tech Stack:** C99, MinGW-w64 (`x86_64-w64-mingw32-gcc`), Winsock2 (`ws2_32`), POSIX Sockets, GNU Make.

## Global Constraints
- Target binary on Windows: `tcp-client.exe` with zero runtime DLL dependencies (links `-lws2_32`).
- Target binary on Linux: `tcp-client`.
- Maintain C99 compliance (`-std=c99 -Wall -Wextra -Werror`).
- Maintain exit code matrix (0: Success, 1: Invalid Args, 2: DNS Fail, 3: Refused, 4: Timeout, 5: Network I/O Error).

---

### Task 1: Create Compatibility Header and Layer (`src/compat.h` & `src/compat.c`)

**Files:**
- Create: `src/compat.h`
- Create: `src/compat.c`

**Interfaces:**
- Produces: `platform_init()`, `platform_cleanup()`, `set_socket_nonblocking(int fd)`, `get_last_socket_error()`, `socket_close(int fd)`, `socket_t` / portability macros.

- [ ] **Step 1: Write `src/compat.h`**

```c
#ifndef COMPAT_H
#define COMPAT_H

#include <stddef.h>
#include <stdbool.h>

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
  #ifndef STDIN_FILENO
  #define STDIN_FILENO 0
  #endif
  #ifndef STDOUT_FILENO
  #define STDOUT_FILENO 1
  #endif
  #ifndef STDERR_FILENO
  #define STDERR_FILENO 2
  #endif

  #define POLLIN  0x0001
  #define POLLOUT 0x0004
  #define POLLERR 0x0008
  #define POLLHUP 0x0010

  typedef WSAPOLLFD pollfd_t;
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
  typedef struct pollfd pollfd_t;
#endif

int platform_init(void);
void platform_cleanup(void);
int set_socket_nonblocking(int sockfd);
int get_last_socket_error(void);
bool is_socket_wouldblock(int err);
bool is_socket_refused(int err);
bool is_socket_timeout(int err);

#endif /* COMPAT_H */
```

- [ ] **Step 2: Write `src/compat.c`**

```c
#include "compat.h"
#include <stdio.h>

#ifdef _WIN32
int platform_init(void) {
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (res != 0) {
        fprintf(stderr, "Error: WSAStartup failed with code %d\n", res);
        return -1;
    }
    return 0;
}

void platform_cleanup(void) {
    WSACleanup();
}

int set_socket_nonblocking(int sockfd) {
    u_long mode = 1;
    return ioctlsocket(sockfd, FIONBIO, &mode);
}

int get_last_socket_error(void) {
    return WSAGetLastError();
}

bool is_socket_wouldblock(int err) {
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
}

bool is_socket_refused(int err) {
    return err == WSAECONNREFUSED || err == WSAEHOSTUNREACH || err == WSAENETUNREACH;
}

bool is_socket_timeout(int err) {
    return err == WSAETIMEDOUT;
}
#else
int platform_init(void) {
    return 0;
}

void platform_cleanup(void) {
}

int set_socket_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int get_last_socket_error(void) {
    return errno;
}

bool is_socket_wouldblock(int err) {
    return err == EAGAIN || err == EWOULDBLOCK || err == EINPROGRESS;
}

bool is_socket_refused(int err) {
    return err == ECONNREFUSED || err == EHOSTUNREACH || err == ENETUNREACH;
}

bool is_socket_timeout(int err) {
    return err == ETIMEDOUT;
}
#endif
```

---

### Task 2: Refactor `socket_client.c`, `signal_handler.c`, `mode_interactive.c`, `mode_oneshot.c`, `main.c`

**Files:**
- Modify: `src/socket_client.c`
- Modify: `src/signal_handler.c`
- Modify: `src/mode_interactive.c`
- Modify: `src/mode_oneshot.c`
- Modify: `src/main.c`

- [ ] **Step 1: Update `src/main.c` to invoke `platform_init()` and `platform_cleanup()`**
- [ ] **Step 2: Update `src/socket_client.c` to use `compat.h` abstraction functions**
- [ ] **Step 3: Update `src/signal_handler.c` for Windows console signal compatibility**
- [ ] **Step 4: Update `src/mode_interactive.c` and `src/mode_oneshot.c` for Windows cross-compatibility**

---

### Task 3: Update `Makefile` for Windows Cross-Compilation

**Files:**
- Modify: `Makefile`

- [ ] **Step 1: Add `compat.c` to `SRCS` and `win` target using `x86_64-w64-mingw32-gcc`**
- [ ] **Step 2: Verify Linux build (`make clean && make test`) passes cleanly**
- [ ] **Step 3: Run `wsl make win` to cross-compile `tcp-client.exe`**
- [ ] **Step 4: Verify `tcp-client.exe` execution in PowerShell on Windows**
