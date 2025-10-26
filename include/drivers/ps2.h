#ifndef DRIVERS_PS2_H
#define DRIVERS_PS2_H

#include <kernel/kernel.h>

/*
 * PS/2 keyboard helper API.
 *
 * ps2_keyboard_init() initializes the PS/2 controller/keyboard.
 * ps2_get_char() returns the next available character (blocking or
 * non-blocking depending on implementation).
 */
kernel_status_t ps2_keyboard_init(void);
char ps2_get_char(void);

#endif /* DRIVERS_PS2_H */