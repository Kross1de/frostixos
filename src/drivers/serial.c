#include <drivers/serial.h>
#include <printf.h>
#include <stdarg.h>

#define SERIAL_PORT 0x3f8

static int serial_is_transmit_empty(void) {
  return inb(SERIAL_PORT + 5) & 0x20;
}

static int serial_is_data_ready(void) { return inb(SERIAL_PORT + 5) & 0x01; }

void serial_set_ansi_fg(vbe_color_t color) {
  serial_printf("\x1b[38;2;%u;%u;%um", color.red, color.green, color.blue);
}

void serial_set_ansi_bg(vbe_color_t color) {
  serial_printf("\x1b[48;2;%u;%u;%um", color.red, color.green, color.blue);
}

kernel_status_t serial_init(void) {
  outb(SERIAL_PORT + 1, 0x00);
  outb(SERIAL_PORT + 3, 0x80);
  outb(SERIAL_PORT + 0, 0x03);
  outb(SERIAL_PORT + 1, 0x00);
  outb(SERIAL_PORT + 3, 0x03);
  outb(SERIAL_PORT + 2, 0xC7);
  outb(SERIAL_PORT + 4, 0x0B);
  outb(SERIAL_PORT + 4, 0x1E);
  outb(SERIAL_PORT + 0, 0xAE);

  if (inb(SERIAL_PORT + 0) != 0xAE) {
    return KERNEL_ERROR;
  }

  outb(SERIAL_PORT + 4, 0x0F);
  return KERNEL_OK;
}

void serial_write_char(char c) {
  while (serial_is_transmit_empty() == 0)
    ;
  outb(SERIAL_PORT, c);
}

void serial_write_string(const char *s) {
  while (*s) {
    serial_write_char(*s++);
  }
}

int serial_read_ready(void) { return serial_is_data_ready() ? 1 : 0; }

int serial_read_char(void) {
  while (!serial_is_data_ready())
    ;
  return (int)inb(SERIAL_PORT);
}

static void serial_out(char c, void *arg) {
  UNUSED(arg);
  if (c) {
    serial_write_char(c);
  }
}

int serial_printf(const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int ret = vfctprintf(serial_out, NULL, fmt, va);
  va_end(va);
  return ret;
}
