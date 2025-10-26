#ifndef MISC_LOGGER_H
#define MISC_LOGGER_H

#include <kernel/kernel.h>

/*
 * Kernel logging facility.
 *
 * log_level_t:
 *  LOG_INFO    - informational messages
 *  LOG_WARN    - warnings that are non-fatal
 *  LOG_ERR     - error conditions
 *  LOG_OKAY    - success/confirmation messages
 */
typedef enum {
	LOG_INFO,
	LOG_WARN,
	LOG_ERR,
	LOG_OKAY,
} log_level_t;

/* Print a log message at the given level. */
void log(log_level_t level, const char *fmt, ...);

#endif /* MISC_LOGGER_H */
