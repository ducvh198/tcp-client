# Architecture Design: Linenoise Integration for Interactive Terminal (`tcp-client`)

**Date**: 2026-09-04  
**Status**: Approved (Transitioning to Implementation Plan)  
**Target Component**: `src/mode_interactive.c`, `src/linenoise.h`, `src/linenoise.c`, `Makefile`

---

## 1. Executive Summary

This specification defines the architecture for upgrading the Interactive Mode of `tcp-client` using a cross-platform, zero-dependency `linenoise` line editing engine. The upgrade introduces:
- Command history navigation using Up/Down arrow keys.
- Tab auto-completion for built-in commands (`exit`, `quit`, `help`, `clear`, `status`, `history`) and common HSM/payment sample commands (`NC0000`, `00 06 30 30 30 30`, `BA`, `CA`, `M0`).
- Persistent command history stored in `~/.tcp_client_history` (or `%USERPROFILE%\.tcp_client_history` on Windows), retaining up to 500 recent entries.
- Real-time socket multiplexing with seamless prompt redraw when asynchronous server messages arrive while the user is actively typing.
- Smart TTY fallback to maintain 100% compatibility with piped input and existing automated E2E test suites.

---

## 2. Architecture & Components

```
+-------------------------------------------------------------+
|                      tcp-client CLI                         |
+-------------------------------------------------------------+
                              |
              +---------------+---------------+
              |                               |
       isatty(STDIN) == true          isatty(STDIN) == false
              |                               |
              v                               v
+-----------------------------+ +-----------------------------+
|  Linenoise Interactive Loop | |  Stream Pipe Reader Loop   |
|  - Raw Terminal / VT Mode   | |  - Standard line streaming  |
|  - Up/Down Arrow History    | |  - Non-blocking poll()      |
|  - Tab Auto-Completion      | |  - Used in automated tests  |
|  - Seamless Reprompt Redraw | +-----------------------------+
+-----------------------------+
              |
     [Event Multiplexer]
     - Keyboard key events
     - Inbound socket data
              |
              v
     ~/.tcp_client_history
```

### 2.1 Cross-Platform Linenoise Engine (`src/linenoise.h`, `src/linenoise.c`)
- **Self-contained, zero-dependency C99 implementation**:
  - **POSIX (Linux/macOS)**: Uses `<termios.h>` to enter/exit raw mode and query terminal dimensions.
  - **Windows**: Uses Win32 Console API (`GetStdHandle`, `GetConsoleMode`, `SetConsoleMode`, `ENABLE_VIRTUAL_TERMINAL_PROCESSING`) to enable VT100 escape sequence processing and translate Windows console input events.
- **Core APIs exposed**:
  - `int linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn);`
  - `void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str);`
  - `int linenoiseHistoryAdd(const char *line);`
  - `int linenoiseHistorySetMaxLen(int len);`
  - `int linenoiseHistorySave(const char *filename);`
  - `int linenoiseHistoryLoad(const char *filename);`
  - `void linenoiseHistoryFree(void);`
  - `int linenoiseEnableRawMode(int fd);`
  - `void linenoiseDisableRawMode(int fd);`
  - `void linenoiseClearScreen(void);`
  - Non-blocking line editor step / state inspection API to support socket polling.

### 2.2 Event Loop & Socket Multiplexing (`src/mode_interactive.c`)
- Checks `isatty(STDIN_FILENO)` (or `_isatty(_fileno(stdin))` on Windows):
  - **Non-TTY**: Executes the classic stream-based line reading loop without terminal raw mode.
  - **TTY**: Enters raw mode and initializes the Linenoise interactive editor state.
- **Multiplexing with 20ms poll slices**:
  - **When Inbound Socket Data Arrives**:
    1. Temporarily erase current prompt & user-typed text: `\r\x1b[2K`.
    2. Write received socket bytes to `stdout` and flush.
    3. Re-draw prompt `> ` and the user's in-progress editing buffer, restoring cursor position `pos`.
  - **When Keyboard Key Event Arrives**:
    - Feeds key into line editor state.
    - **Tab**: Triggers `linenoise_completion_callback()`.
    - **Up / Down Arrows**: Traverses command history buffer.
    - **Enter (`\r` / `\n`)**:
      - Normalizes line string.
      - If `exit` or `quit`: Exits cleanly with return code 0.
      - If built-in command (`help`, `clear`, `status`, `history`): Executes locally and reprompts.
      - Otherwise: Sends line over socket via `socket_write_all()`, appends to history, and saves to file.

### 2.3 Auto-Completion Directory
The completion callback inspects the buffer prefix up to cursor position and matches against:
1. **Control Commands**:
   - `exit`, `quit`, `help`, `clear`, `status`, `history`
2. **HSM & Payment Samples**:
   - `NC0000` (Diagnostics)
   - `00 06 30 30 30 30` (TCP Length + Hex Echo)
   - `BA` (Translate PIN LMK to ZPK)
   - `BB` (Translate PIN ZPK to LMK)
   - `CA` (Verify Terminal PIN)
   - `CB` (Generate PIN Block)
   - `M0` (Generate MAC)
   - `M2` (Verify MAC)

### 2.4 Persistent History Management
- File resolution:
  - POSIX: `$HOME/.tcp_client_history`
  - Windows: `%USERPROFILE%\.tcp_client_history` (fallback: `./.tcp_client_history`)
- Capacity: 500 entries.
- Loaded upon entering interactive mode, saved incrementally after every executed command and during session exit.

---

## 3. Build & Packaging Changes
- `Makefile`: Add `src/linenoise.c` to `SRCS`.
- No third-party packages or dynamic libraries needed. Compiles with standard `gcc` (Linux) and `x86_64-w64-mingw32-gcc` (Windows MinGW).

---

## 4. Verification Plan

### Automated Regression Verification
- Run `python tests/test_runner.py` (all 65/65 tests must pass).
- Run `python tests/test_adversarial.py` (all 14/14 tests must pass).
- Run `python tests/stress_cli_parser.py` (all 34/34 tests must pass).

### Unit & Integration Verification
- Add unit tests verifying completion callback matching and history save/load functionality.

### Manual Verification
- Test interactive typing in Windows Terminal / Command Prompt:
  - Arrow Up/Down cycles previous commands.
  - Pressing Tab completes `ex` to `exit` and `he` to `help`.
  - Re-running client loads previous session history.
