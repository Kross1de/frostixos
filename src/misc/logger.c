#include <kernel/kernel.h>
#include <printf.h>
#include <misc/logger.h>
#include <lib/terminal.h>
#include <drivers/vbe.h>

static const char* level_str[] = {
    [LOG_INFO] = "INFO",
    [LOG_WARN] = "WARN",
    [LOG_ERR]  = "ERR",
    [LOG_OKAY] = "OKAY"
};

void log(log_level_t level, const char* fmt, ...) {
    char buf[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);

    vbe_color_t original_color = g_terminal.fg_color;
    switch (level) {
        case LOG_INFO:
            terminal_set_fg_color(&g_terminal, VBE_COLOR_CYAN);
            break;
        case LOG_WARN:
            terminal_set_fg_color(&g_terminal, VBE_COLOR_YELLOW);
            break;
        case LOG_ERR:
            terminal_set_fg_color(&g_terminal, VBE_COLOR_RED);
            break;
        case LOG_OKAY:
            terminal_set_fg_color(&g_terminal, VBE_COLOR_GREEN);
            break;
        default:
            terminal_set_fg_color(&g_terminal, VBE_COLOR_WHITE);
            break;
    }

    printf("[%s] %s\n", level_str[level], buf);

    terminal_set_fg_color(&g_terminal, original_color);
}
