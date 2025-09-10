#ifndef SERIAL_H
#define SERIAL_H

#include <kernel/kernel.h>

kernel_status_t serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *s);
int serial_printf(const char *fmt, ...);

#endif
