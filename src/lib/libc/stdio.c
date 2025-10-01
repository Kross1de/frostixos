#include <drivers/serial.h>
#include <printf.h>
#include "stdio.h"

int putchar(int c) {
  _putchar((char)c);
  return (unsigned char)c;
}

int getchar(void) {
  return serial_read_char();
}