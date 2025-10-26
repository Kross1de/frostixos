#ifndef SERIAL_H
#define SERIAL_H

#include <drivers/vbe.h>
#include <kernel/kernel.h>

/*
 * Serial I/O helpers for debug and kernel logging.
 *
 * These functions abstract a serial port for writes and basic reads and
 * support rudimentary ANSI color control via set_ansi_fg/bg.
 */
kernel_status_t serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *s);
int serial_printf(const char *fmt, ...);

int serial_read_ready(void);
int serial_read_char(void);

void serial_set_ansi_fg(vbe_color_t color);
void serial_set_ansi_bg(vbe_color_t color);

#endif /* SERIAL_H */