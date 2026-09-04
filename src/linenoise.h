#ifndef LINENOISE_H
#define LINENOISE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct linenoiseCompletions {
    size_t len;
    char **cvec;
} linenoiseCompletions;

typedef void(linenoiseCompletionCallback)(const char *, linenoiseCompletions *);
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn);
void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str);
void linenoiseFreeCompletions(linenoiseCompletions *lc);

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

char *linenoise(const char *prompt);
void linenoiseFree(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* LINENOISE_H */
