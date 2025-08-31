#ifndef MISC_LOGGER_H
#define MISC_LOGGER_H

#include <kernel/kernel.h>

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERR,
    LOG_OKAY
} log_level_t;

void log(log_level_t level, const char* fmt, ...);

#endif // MISC_LOGGER_H
