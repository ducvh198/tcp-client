#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <stdbool.h>

/**
 * Initializes POSIX signal handling (SIGPIPE suppression, SIGINT/SIGTERM catching)
 * and registers atexit() terminal attribute restoration.
 */
void setup_signal_handlers(void);

/**
 * Returns true (non-zero) if an interrupt signal (SIGINT or SIGTERM) was caught.
 *
 * @return 1 if interrupted, 0 otherwise.
 */
int signal_handler_is_interrupted(void);

#endif /* SIGNAL_HANDLER_H */
