#ifndef SERIAL_H
#define SERIAL_H

#include <drivers/vbe.h>
#include <kernel/kernel.h>

kernel_status_t serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *s);
int serial_printf(const char *fmt, ...);

int serial_read_ready(void);
int serial_read_char(void);

void serial_set_ansi_fg(vbe_color_t color);
void serial_set_ansi_bg(vbe_color_t color);

#endif
