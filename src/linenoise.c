#include "linenoise.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <conio.h>
  #include <io.h>

  #ifndef STDIN_FILENO
  #define STDIN_FILENO 0
  #endif
  #ifndef STDOUT_FILENO
  #define STDOUT_FILENO 1
  #endif

  #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
  #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
  #endif
  #ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
  #define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
  #endif
  #ifndef DISABLE_NEWLINE_AUTO_RETURN
  #define DISABLE_NEWLINE_AUTO_RETURN 0x0008
  #endif
#else
  #include <unistd.h>
  #include <termios.h>
#endif

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 500

static linenoiseCompletionCallback *completionCallback = NULL;
static linenoiseCompletions cached_completions = {0, NULL};

static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;
static char *saved_line = NULL;

static int rawmode = 0;
static int atexit_registered = 0;

#ifdef _WIN32
static HANDLE hStdin = INVALID_HANDLE_VALUE;
static HANDLE hStdout = INVALID_HANDLE_VALUE;
static DWORD orig_stdin_mode = 0;
static DWORD orig_stdout_mode = 0;
#else
static struct termios orig_termios;
#endif

typedef enum {
    ESC_STATE_NORMAL = 0,
    ESC_STATE_ESC,
    ESC_STATE_CSI,
    ESC_STATE_CSI_NUM,
    ESC_STATE_SS3,
    ESC_STATE_WIN_EXT
} esc_state_t;

static esc_state_t esc_state = ESC_STATE_NORMAL;
static int csi_num = 0;

static char *linenoise_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn) {
    completionCallback = fn;
}

void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
    if (!lc || !str) return;
    char *copy = linenoise_strdup(str);
    if (!copy) return;
    char **new_cvec = (char **)realloc(lc->cvec, sizeof(char *) * (lc->len + 1));
    if (!new_cvec) {
        free(copy);
        return;
    }
    lc->cvec = new_cvec;
    lc->cvec[lc->len] = copy;
    lc->len++;
}

void linenoiseFreeCompletions(linenoiseCompletions *lc) {
    if (!lc) return;
    if (lc->cvec) {
        for (size_t i = 0; i < lc->len; i++) {
            free(lc->cvec[i]);
        }
        free(lc->cvec);
        lc->cvec = NULL;
    }
    lc->len = 0;
}

int linenoiseHistorySetMaxLen(int len) {
    if (len < 1) return 0;
    if (history) {
        int to_remove = history_len - len;
        if (to_remove > 0) {
            for (int i = 0; i < to_remove; i++) {
                free(history[i]);
            }
            for (int i = 0; i < len; i++) {
                history[i] = history[i + to_remove];
            }
            history_len = len;
        }
        char **new_hist = (char **)realloc(history, sizeof(char *) * len);
        if (new_hist) {
            history = new_hist;
        }
    }
    history_max_len = len;
    return 1;
}

int linenoiseHistoryAdd(const char *line) {
    if (!line || line[0] == '\0' || history_max_len <= 0) {
        return 0;
    }
    if (history && history_len > 0 && strcmp(history[history_len - 1], line) == 0) {
        return 0;
    }
    char *copy = linenoise_strdup(line);
    if (!copy) {
        return 0;
    }
    if (history_len >= history_max_len) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (history_max_len - 1));
        history[history_max_len - 1] = copy;
        return 1;
    }
    char **new_hist = (char **)realloc(history, sizeof(char *) * (history_len + 1));
    if (!new_hist) {
        free(copy);
        return 0;
    }
    history = new_hist;
    history[history_len] = copy;
    history_len++;
    return 1;
}

int linenoiseHistorySave(const char *filename) {
    if (!filename) return -1;
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;
    for (int i = 0; i < history_len; i++) {
        if (history[i]) {
            fprintf(fp, "%s\n", history[i]);
        }
    }
    fclose(fp);
    return 0;
}

int linenoiseHistoryLoad(const char *filename) {
    if (!filename) return -1;
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        size_t l = strlen(buf);
        while (l > 0 && (buf[l - 1] == '\r' || buf[l - 1] == '\n')) {
            buf[l - 1] = '\0';
            l--;
        }
        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}

void linenoiseHistoryFree(void) {
    if (history) {
        for (int i = 0; i < history_len; i++) {
            free(history[i]);
        }
        free(history);
        history = NULL;
    }
    history_len = 0;
    if (saved_line) {
        free(saved_line);
        saved_line = NULL;
    }
}

static void linenoiseAtExit(void) {
    linenoiseDisableRawMode(STDIN_FILENO);
}

int linenoiseEnableRawMode(int fd) {
    (void)fd;
    if (rawmode) return 0;

#ifdef _WIN32
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
        return -1;
    }
    if (!GetConsoleMode(hStdin, &orig_stdin_mode)) {
        return -1;
    }
    if (!GetConsoleMode(hStdout, &orig_stdout_mode)) {
        return -1;
    }

    DWORD raw_in = orig_stdin_mode;
    raw_in &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    raw_in |= ENABLE_VIRTUAL_TERMINAL_INPUT;

    DWORD raw_out = orig_stdout_mode;
    raw_out |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    raw_out |= DISABLE_NEWLINE_AUTO_RETURN;

    if (!SetConsoleMode(hStdin, raw_in)) {
        return -1;
    }
    SetConsoleMode(hStdout, raw_out);
#else
    if (!isatty(fd)) return -1;
    if (tcgetattr(fd, &orig_termios) == -1) return -1;

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) return -1;
#endif

    rawmode = 1;
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }
    return 0;
}

void linenoiseDisableRawMode(int fd) {
    (void)fd;
    if (!rawmode) return;

#ifdef _WIN32
    if (hStdin != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hStdin, orig_stdin_mode);
    }
    if (hStdout != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hStdout, orig_stdout_mode);
    }
#else
    tcsetattr(fd, TCSAFLUSH, &orig_termios);
#endif
    rawmode = 0;
}

void linenoiseClearScreen(void) {
    fputs("\x1b[H\x1b[2J", stdout);
    fflush(stdout);
}

void linenoiseEditRedraw(linenoiseState *l) {
    if (!l) return;
    fputs("\r\x1b[2K", stdout);
    if (l->prompt && l->plen > 0) {
        fputs(l->prompt, stdout);
    }
    if (l->buf && l->len > 0) {
        fwrite(l->buf, 1, l->len, stdout);
    }
    if (l->len > l->pos) {
        fprintf(stdout, "\x1b[%dD", (int)(l->len - l->pos));
    }
    fflush(stdout);
}

void linenoiseEditStart(linenoiseState *l, char *buf, size_t buflen, const char *prompt) {
    if (!l || !buf || buflen == 0) return;
    l->in_completion = 0;
    l->completion_idx = 0;
    l->buf = buf;
    l->buflen = buflen;
    l->prompt = prompt ? prompt : "";
    l->plen = strlen(l->prompt);
    l->pos = 0;
    l->len = 0;
    l->buf[0] = '\0';
    l->history_index = (size_t)history_len;

    if (saved_line) {
        free(saved_line);
        saved_line = NULL;
    }
    esc_state = ESC_STATE_NORMAL;
    csi_num = 0;
    linenoiseFreeCompletions(&cached_completions);
    linenoiseEditRedraw(l);
}

void linenoiseEditStop(linenoiseState *l) {
    if (!l) return;
    l->in_completion = 0;
    l->completion_idx = 0;
    esc_state = ESC_STATE_NORMAL;
    csi_num = 0;
    linenoiseFreeCompletions(&cached_completions);
    if (saved_line) {
        free(saved_line);
        saved_line = NULL;
    }
}

int linenoiseEditFeed(linenoiseState *l, int c) {
    if (!l || !l->buf || l->buflen == 0) return 0;
    if (c == -1) return -1;

    if (esc_state == ESC_STATE_ESC) {
        if (c == '[') {
            esc_state = ESC_STATE_CSI;
            return 0;
        } else if (c == 'O') {
            esc_state = ESC_STATE_SS3;
            return 0;
        }
        esc_state = ESC_STATE_NORMAL;
        return 0;
    }

    if (esc_state == ESC_STATE_CSI) {
        if (c == 'A') {
            esc_state = ESC_STATE_NORMAL;
            goto handle_up;
        } else if (c == 'B') {
            esc_state = ESC_STATE_NORMAL;
            goto handle_down;
        } else if (c == 'C') {
            esc_state = ESC_STATE_NORMAL;
            goto handle_right;
        } else if (c == 'D') {
            esc_state = ESC_STATE_NORMAL;
            goto handle_left;
        } else if (c == 'H') {
            esc_state = ESC_STATE_NORMAL;
            goto handle_home;
        } else if (c == 'F') {
            esc_state = ESC_STATE_NORMAL;
            goto handle_end;
        } else if (c >= '0' && c <= '9') {
            csi_num = c - '0';
            esc_state = ESC_STATE_CSI_NUM;
            return 0;
        }
        esc_state = ESC_STATE_NORMAL;
        return 0;
    }

    if (esc_state == ESC_STATE_CSI_NUM) {
        if (c == '~') {
            esc_state = ESC_STATE_NORMAL;
            if (csi_num == 3) {
                goto handle_delete;
            } else if (csi_num == 1 || csi_num == 7) {
                goto handle_home;
            } else if (csi_num == 4 || csi_num == 8) {
                goto handle_end;
            }
            return 0;
        } else if (c >= '0' && c <= '9') {
            csi_num = csi_num * 10 + (c - '0');
            return 0;
        }
        esc_state = ESC_STATE_NORMAL;
        return 0;
    }

    if (esc_state == ESC_STATE_SS3) {
        esc_state = ESC_STATE_NORMAL;
        if (c == 'A') goto handle_up;
        if (c == 'B') goto handle_down;
        if (c == 'C') goto handle_right;
        if (c == 'D') goto handle_left;
        if (c == 'H') goto handle_home;
        if (c == 'F') goto handle_end;
        return 0;
    }

    if (esc_state == ESC_STATE_WIN_EXT) {
        esc_state = ESC_STATE_NORMAL;
        if (c == 72) goto handle_up;
        if (c == 80) goto handle_down;
        if (c == 75) goto handle_left;
        if (c == 77) goto handle_right;
        if (c == 71) goto handle_home;
        if (c == 79) goto handle_end;
        if (c == 83) goto handle_delete;
        return 0;
    }

    if (c == 27) {
        if (l->in_completion) {
            l->in_completion = 0;
            l->completion_idx = 0;
            linenoiseFreeCompletions(&cached_completions);
        }
        esc_state = ESC_STATE_ESC;
        return 0;
    }

    if (c == 0 || (unsigned char)c == 0xE0) {
        if (l->in_completion) {
            l->in_completion = 0;
            l->completion_idx = 0;
            linenoiseFreeCompletions(&cached_completions);
        }
        esc_state = ESC_STATE_WIN_EXT;
        return 0;
    }

    if (c == '\t' || c == 9) {
        if (!completionCallback) {
            return 0;
        }
        if (!l->in_completion) {
            linenoiseFreeCompletions(&cached_completions);
            completionCallback(l->buf, &cached_completions);
            if (cached_completions.len == 0) {
                return 0;
            }
            l->in_completion = 1;
            l->completion_idx = 0;
        } else {
            l->completion_idx = (l->completion_idx + 1) % cached_completions.len;
        }

        const char *comp = cached_completions.cvec[l->completion_idx];
        size_t complen = strlen(comp);
        if (complen >= l->buflen) complen = l->buflen - 1;
        memcpy(l->buf, comp, complen);
        l->buf[complen] = '\0';
        l->len = complen;
        l->pos = complen;
        linenoiseEditRedraw(l);
        return 0;
    }

    if (l->in_completion) {
        l->in_completion = 0;
        l->completion_idx = 0;
        linenoiseFreeCompletions(&cached_completions);
    }

    if (c == '\r' || c == '\n') {
        l->buf[l->len] = '\0';
        fputs("\r\n", stdout);
        fflush(stdout);
        return 1;
    }

    if (c == '\b' || c == 8 || c == 127) {
        if (l->pos > 0) {
            memmove(l->buf + l->pos - 1, l->buf + l->pos, l->len - l->pos + 1);
            l->pos--;
            l->len--;
            linenoiseEditRedraw(l);
        }
        return 0;
    }

    if (c == 3) {
        l->buf[0] = '\0';
        l->len = 0;
        l->pos = 0;
        fputs("^C\r\n", stdout);
        fflush(stdout);
        return -1;
    }

    if (c == 4) {
        if (l->len == 0) {
            return -1;
        }
        goto handle_delete;
    }

    if (c == 21) {
        l->buf[0] = '\0';
        l->len = 0;
        l->pos = 0;
        linenoiseEditRedraw(l);
        return 0;
    }

    if (c == 11) {
        l->buf[l->pos] = '\0';
        l->len = l->pos;
        linenoiseEditRedraw(l);
        return 0;
    }

    if (c == 1) {
        goto handle_home;
    }

    if (c == 5) {
        goto handle_end;
    }

    if (c == 12) {
        linenoiseClearScreen();
        linenoiseEditRedraw(l);
        return 0;
    }

    if (c >= 32 && (unsigned char)c != 127) {
        if (l->len < l->buflen - 1) {
            if (l->pos < l->len) {
                memmove(l->buf + l->pos + 1, l->buf + l->pos, l->len - l->pos);
            }
            l->buf[l->pos] = (char)c;
            l->pos++;
            l->len++;
            l->buf[l->len] = '\0';
            linenoiseEditRedraw(l);
        }
        return 0;
    }

    return 0;

handle_up:
    if (history_len > 0 && l->history_index > 0) {
        if (l->history_index == (size_t)history_len) {
            if (saved_line) free(saved_line);
            saved_line = linenoise_strdup(l->buf);
        }
        l->history_index--;
        const char *h = history[l->history_index];
        size_t hlen = strlen(h);
        if (hlen >= l->buflen) hlen = l->buflen - 1;
        memcpy(l->buf, h, hlen);
        l->buf[hlen] = '\0';
        l->len = hlen;
        l->pos = hlen;
        linenoiseEditRedraw(l);
    }
    return 0;

handle_down:
    if (history_len > 0 && l->history_index < (size_t)history_len) {
        l->history_index++;
        const char *h = (l->history_index == (size_t)history_len)
                            ? (saved_line ? saved_line : "")
                            : history[l->history_index];
        size_t hlen = strlen(h);
        if (hlen >= l->buflen) hlen = l->buflen - 1;
        memcpy(l->buf, h, hlen);
        l->buf[hlen] = '\0';
        l->len = hlen;
        l->pos = hlen;
        linenoiseEditRedraw(l);
    }
    return 0;

handle_left:
    if (l->pos > 0) {
        l->pos--;
        linenoiseEditRedraw(l);
    }
    return 0;

handle_right:
    if (l->pos < l->len) {
        l->pos++;
        linenoiseEditRedraw(l);
    }
    return 0;

handle_home:
    l->pos = 0;
    linenoiseEditRedraw(l);
    return 0;

handle_end:
    l->pos = l->len;
    linenoiseEditRedraw(l);
    return 0;

handle_delete:
    if (l->pos < l->len) {
        memmove(l->buf + l->pos, l->buf + l->pos + 1, l->len - l->pos);
        l->len--;
        linenoiseEditRedraw(l);
    }
    return 0;
}

char *linenoise(const char *prompt) {
    char buf[4096];
    linenoiseState l;
    if (linenoiseEnableRawMode(STDIN_FILENO) == -1) {
        fputs(prompt, stdout);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL) return NULL;
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) {
            buf[len - 1] = '\0';
            len--;
        }
        return linenoise_strdup(buf);
    }

    linenoiseEditStart(&l, buf, sizeof(buf), prompt);
    while (1) {
        int c;
#ifdef _WIN32
        c = _getch();
#else
        char ch;
        int nread = read(STDIN_FILENO, &ch, 1);
        if (nread <= 0) {
            c = -1;
        } else {
            c = (unsigned char)ch;
        }
#endif
        if (c == -1) {
            linenoiseEditStop(&l);
            linenoiseDisableRawMode(STDIN_FILENO);
            return NULL;
        }
        int res = linenoiseEditFeed(&l, c);
        if (res == 1) {
            linenoiseEditStop(&l);
            linenoiseDisableRawMode(STDIN_FILENO);
            return linenoise_strdup(l.buf);
        } else if (res == -1) {
            linenoiseEditStop(&l);
            linenoiseDisableRawMode(STDIN_FILENO);
            return NULL;
        }
    }
}

void linenoiseFree(void *ptr) {
    free(ptr);
}
