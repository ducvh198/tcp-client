#include "signal_handler.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static volatile sig_atomic_t g_interrupted = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_interrupted = 1;
}

#ifndef _WIN32
#include <termios.h>
static struct termios g_orig_termios;
static bool g_termios_saved = false;

static void restore_terminal(void) {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
    }
}
#endif

void setup_signal_handlers(void) {
#ifndef _WIN32
    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
            g_termios_saved = true;
            atexit(restore_terminal);
        }
    }

    #ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    #endif

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#else
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
}

int signal_handler_is_interrupted(void) {
    return g_interrupted != 0;
}
