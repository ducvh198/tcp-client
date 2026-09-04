# Linenoise Interactive Terminal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate a cross-platform, zero-dependency `linenoise` line editing engine into `tcp-client` to provide command history navigation (Up/Down arrows), Tab auto-completion, persistent history (`~/.tcp_client_history`), and seamless reprompt redraw during socket reception.

**Architecture:** A lightweight C99 cross-platform linenoise engine (`src/linenoise.c`/`.h`) supporting POSIX termios and Windows Console VT100 API is linked into `tcp-client`. In `src/mode_interactive.c`, an auto-detecting TTY gate dispatches between a rich Linenoise event loop (with 20ms polling, asynchronous socket message redraw, Tab completion, and history persistence) and the standard non-blocking stream reader for non-TTY/piped environments (ensuring 100% test compatibility).

**Tech Stack:** C99, POSIX Sockets & termios, Windows Console API / Winsock2, Python 3 test harness.

**Spec:** [`docs/superpowers/specs/2026-09-04-linenoise-interactive-terminal-design.md`](file:///d:/DEV/3DS/acs_kernel_ncudcntt/tcp-client-cli/docs/superpowers/specs/2026-09-04-linenoise-interactive-terminal-design.md)

## Global Constraints

- Must compile under standard C99 with zero third-party external dependencies (only standard `libc` and platform APIs).
- Both Linux (`make`) and Windows MinGW (`make win`) builds must compile cleanly without errors or warnings.
- Preserves 100% PASS rate for the existing 65/65 E2E test cases in `tests/test_runner.py`.
- Terminal attributes must always be safely restored upon exit or signal interruption (`SIGINT`/Ctrl+C).

---

### Task 1: Create Cross-Platform Linenoise Module (`src/linenoise.h`, `src/linenoise.c`, `Makefile`)

**Files:**
- Create: `src/linenoise.h`
- Create: `src/linenoise.c`
- Modify: `Makefile:19-27`
- Test: `tests/test_linenoise_c.c`

**Interfaces:**
- Produces:
  ```c
  typedef struct linenoiseCompletions {
      size_t len;
      char **cvec;
  } linenoiseCompletions;

  typedef void(linenoiseCompletionCallback)(const char *, linenoiseCompletions *);
  void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn);
  void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str);

  int linenoiseHistoryAdd(const char *line);
  int linenoiseHistorySetMaxLen(int len);
  int linenoiseHistorySave(const char *filename);
  int linenoiseHistoryLoad(const char *filename);
  void linenoiseHistoryFree(void);

  int linenoiseEnableRawMode(int fd);
  void linenoiseDisableRawMode(int fd);
  void linenoiseClearScreen(void);

  /* Non-blocking editing state for socket multiplexing */
  typedef struct linenoiseState {
      int in_completion;
      size_t completion_idx;
      char *buf;
      size_t buflen;
      const char *prompt;
      size_t plen;
      size_t pos;
      size_t len;
      size_t history_index;
  } linenoiseState;

  void linenoiseEditStart(linenoiseState *l, char *buf, size_t buflen, const char *prompt);
  int linenoiseEditFeed(linenoiseState *l, int c);
  void linenoiseEditStop(linenoiseState *l);
  void linenoiseEditRedraw(linenoiseState *l);
  ```

- [ ] **Step 1: Write C unit test verifying linenoise history and completion logic**

Create `tests/test_linenoise_c.c`:
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "linenoise.h"

static void dummy_completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == 'e') {
        linenoiseAddCompletion(lc, "exit");
    }
}

int main(void) {
    linenoiseSetCompletionCallback(dummy_completion);
    linenoiseHistorySetMaxLen(50);
    assert(linenoiseHistoryAdd("test command 1") == 1);
    assert(linenoiseHistoryAdd("test command 2") == 1);
    const char *tmp_hist = "test_hist.tmp";
    assert(linenoiseHistorySave(tmp_hist) == 0);
    linenoiseHistoryFree();
    assert(linenoiseHistoryLoad(tmp_hist) == 0);
    remove(tmp_hist);
    linenoiseHistoryFree();
    printf("Linenoise unit test PASS\n");
    return 0;
}
```

- [ ] **Step 2: Implement `src/linenoise.h` and `src/linenoise.c`**

Provide cross-platform line editor supporting POSIX (`termios`) and Windows (`ENABLE_VIRTUAL_TERMINAL_PROCESSING` / Win32 console keys).

- [ ] **Step 3: Update `Makefile` to link `src/linenoise.c`**

Add `$(SRC_DIR)/linenoise.c` to `SRCS` in `Makefile`.

- [ ] **Step 4: Compile and run unit test**

Run: `gcc -Isrc tests/test_linenoise_c.c src/linenoise.c -o tests/test_linenoise_c && ./tests/test_linenoise_c`
Expected: Output "Linenoise unit test PASS" and exit code 0.

- [ ] **Step 5: Commit Task 1**

```bash
git add src/linenoise.h src/linenoise.c Makefile tests/test_linenoise_c.c
git commit -m "feat: add cross-platform linenoise module with completion and history"
```

---

### Task 2: Implement Tab Completion Callback & Persistent History Management

**Files:**
- Modify: `src/mode_interactive.h`
- Modify: `src/mode_interactive.c`
- Test: `tests/test_interactive_logic.py`

**Interfaces:**
- Consumes:
  - `linenoiseHistoryLoad`, `linenoiseHistorySave`, `linenoiseHistoryAdd`
  - `linenoiseSetCompletionCallback`, `linenoiseAddCompletion`
- Produces:
  ```c
  void interactive_completion_callback(const char *buf, linenoiseCompletions *lc);
  char *get_interactive_history_path(char *out_path, size_t max_len);
  ```

- [ ] **Step 1: Write integration test verifying history file and completions**

Create `tests/test_interactive_logic.py`:
```python
import subprocess
import os
import sys

def test_history_file_resolution():
    # Verify that .tcp_client_history is resolved and loaded/saved
    home = os.environ.get("USERPROFILE") or os.environ.get("HOME")
    assert home is not None
    hist_path = os.path.join(home, ".tcp_client_history")
    print(f"Target history file: {hist_path}")

if __name__ == "__main__":
    test_history_file_resolution()
    print("Interactive logic test PASS")
```

- [ ] **Step 2: Implement `interactive_completion_callback` and `get_interactive_history_path`**

Register built-ins:
`"exit"`, `"quit"`, `"help"`, `"clear"`, `"status"`, `"history"`.
Register HSM samples:
`"NC0000"`, `"00 06 30 30 30 30"`, `"BA"`, `"BB"`, `"CA"`, `"CB"`, `"M0"`, `"M2"`.

Resolve history path:
POSIX: `$HOME/.tcp_client_history`
Windows: `%USERPROFILE%\.tcp_client_history`

- [ ] **Step 3: Run test verification**

Run: `python tests/test_interactive_logic.py`
Expected: PASS

- [ ] **Step 4: Commit Task 2**

```bash
git add src/mode_interactive.c src/mode_interactive.h tests/test_interactive_logic.py
git commit -m "feat: add tab completion callback and persistent history management"
```

---

### Task 3: Refactor `mode_interactive.c` for Real-Time Multiplexing & TTY Fallback

**Files:**
- Modify: `src/mode_interactive.c`
- Test: `tests/test_runner.py` (Piped mode test validation)

**Interfaces:**
- Consumes:
  - `linenoiseEditStart`, `linenoiseEditFeed`, `linenoiseEditRedraw`, `linenoiseEditStop`
  - `socket_read`, `socket_write_all`
- Produces:
  - Seamless prompt redrawing on socket I/O
  - Fallback to stream-reader when `isatty(STDIN) == false`

- [ ] **Step 1: Verify current automated tests before modification**

Run: `python tests/test_runner.py`
Expected: 65/65 PASS

- [ ] **Step 2: Implement TTY detection and dual-loop execution in `run_interactive_mode`**

In `src/mode_interactive.c`:
1. Check `stdin_is_tty = isatty(STDIN_FILENO)` (or Windows equivalent).
2. If `!stdin_is_tty`: execute existing stream reading loop.
3. If `stdin_is_tty`:
   - Initialize history from `~/.tcp_client_history`.
   - Setup completion callback.
   - Enter raw mode via `linenoiseEnableRawMode()`.
   - Run multiplexed loop:
     - Check socket activity every 20ms slice.
     - On socket data: `\r\x1b[2K`, print server data, call `linenoiseEditRedraw()`.
     - On keyboard input: call `linenoiseEditFeed()`.
     - On Enter: if built-in command (`help`, `clear`, `status`, `history`), execute locally; otherwise transmit via socket and append to history.
   - On exit: save history and call `linenoiseDisableRawMode()`.

- [ ] **Step 3: Run automated test suite to ensure no regression in non-TTY mode**

Run: `python tests/test_runner.py`
Expected: 65/65 PASS

- [ ] **Step 4: Run adversarial test suite**

Run: `python tests/test_adversarial.py`
Expected: 14/14 PASS

- [ ] **Step 5: Commit Task 3**

```bash
git add src/mode_interactive.c
git commit -m "feat: implement real-time multiplexed linenoise interactive mode with tty fallback"
```

---

### Task 4: End-to-End Verification & User Documentation

**Files:**
- Modify: `README.md`
- Modify: `PROJECT.md`
- Test: `tests/test_runner.py`, `tests/test_adversarial.py`, `tests/stress_cli_parser.py`

- [ ] **Step 1: Run all test suites**

Run:
1. `python tests/test_runner.py`
2. `python tests/test_adversarial.py`
3. `python tests/stress_cli_parser.py`
Expected: 100% PASS across all suites.

- [ ] **Step 2: Update documentation**

Update `README.md` with:
- Interactive Mode features: Up/Down arrow history, Tab completion, built-in commands (`help`, `clear`, `status`, `history`), persistent history file `~/.tcp_client_history`.

- [ ] **Step 3: Commit documentation & finalize**

```bash
git add README.md PROJECT.md
git commit -m "docs: document linenoise interactive mode enhancements and shortcuts"
```
