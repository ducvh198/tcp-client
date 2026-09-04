#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "linenoise.h"

static void dummy_completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == 'e') {
        linenoiseAddCompletion(lc, "exit");
        linenoiseAddCompletion(lc, "echo");
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

    /* Test completion list allocation and cleanup */
    linenoiseCompletions lc = {0, NULL};
    dummy_completion("e", &lc);
    assert(lc.len == 2);
    assert(strcmp(lc.cvec[0], "exit") == 0);
    assert(strcmp(lc.cvec[1], "echo") == 0);
    linenoiseFreeCompletions(&lc);
    assert(lc.len == 0);
    assert(lc.cvec == NULL);

    /* Test non-blocking editing feed */
    char buf[64];
    linenoiseState l;
    linenoiseEditStart(&l, buf, sizeof(buf), "> ");
    assert(linenoiseEditFeed(&l, 'h') == 0);
    assert(linenoiseEditFeed(&l, 'e') == 0);
    assert(linenoiseEditFeed(&l, 'l') == 0);
    assert(linenoiseEditFeed(&l, 'p') == 0);
    assert(l.len == 4);
    assert(strcmp(l.buf, "help") == 0);

    /* Backspace test */
    assert(linenoiseEditFeed(&l, '\b') == 0);
    assert(l.len == 3);
    assert(strcmp(l.buf, "hel") == 0);

    /* Enter test */
    assert(linenoiseEditFeed(&l, '\n') == 1);
    assert(strcmp(l.buf, "hel") == 0);
    linenoiseEditStop(&l);

    printf("Linenoise unit test PASS\n");
    return 0;
}
