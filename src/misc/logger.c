#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/terminal.h>
#include <misc/logger.h>
#include <printf.h>

static const char *level_str[] = {[LOG_INFO] = "INFO",
                                  [LOG_WARN] = "WARN",
                                  [LOG_ERR] = "ERR",
                                  [LOG_OKAY] = "OKAY"};

void log(log_level_t level, const char *fmt, ...) {
  char buf[1024];
  va_list va;
  va_start(va, fmt);
  vsnprintf(buf, sizeof(buf), fmt, va);
  va_end(va);

  const char *color_code;
  switch (level) {
  case LOG_INFO:
    color_code = "\x1b[36m";
    break;
  case LOG_WARN:
    color_code = "\x1b[33m";
    break;
  case LOG_ERR:
    color_code = "\x1b[31m";
    break;
  case LOG_OKAY:
    color_code = "\x1b[32m";
    break;
  default:
    color_code = "\x1b[37m";
    break;
  }

  printf("%s[%s] \x1b[37m%s\n", color_code, level_str[level], buf);
}