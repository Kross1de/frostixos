#include "stdio.h"
#include <drivers/serial.h>
#include <printf.h>

/* Output a character to the kernel console/serial */
int putchar(int c)
{
	_putchar((char)c);
	return (unsigned char)c;
}

/* Blocking input from the main serial port */
int getchar(void)
{
	return serial_read_char();
}