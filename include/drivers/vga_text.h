#ifndef DRIVERS_VGA_TEXT_H
#define DRIVERS_VGA_TEXT_H

#include <kernel/kernel.h>

/*
 * Legacy VGA text mode helpers.
 *
 * This API provides an abstraction for writing characters to the VGA text
 * buffer, managing cursor position and simple scrolling.
 */

#define VGA_TEXT_WIDTH 80
#define VGA_TEXT_HEIGHT 25

typedef enum {
	VGA_TEXT_COLOR_BLACK = 0,
	VGA_TEXT_COLOR_BLUE = 1,
	VGA_TEXT_COLOR_GREEN = 2,
	VGA_TEXT_COLOR_CYAN = 3,
	VGA_TEXT_COLOR_RED = 4,
	VGA_TEXT_COLOR_MAGENTA = 5,
	VGA_TEXT_COLOR_BROWN = 6,
	VGA_TEXT_COLOR_LIGHT_GREY = 7,
	VGA_TEXT_COLOR_DARK_GREY = 8,
	VGA_TEXT_COLOR_LIGHT_BLUE = 9,
	VGA_TEXT_COLOR_LIGHT_GREEN = 10,
	VGA_TEXT_COLOR_LIGHT_CYAN = 11,
	VGA_TEXT_COLOR_LIGHT_RED = 12,
	VGA_TEXT_COLOR_LIGHT_MAGENTA = 13,
	VGA_TEXT_COLOR_LIGHT_BROWN = 14,
	VGA_TEXT_COLOR_WHITE = 15,
} vga_text_color_t;

/* Simple cursor struct */
typedef struct {
	u16 x;
	u16 y;
} vga_text_cursor_t;

/* VGA text device state */
typedef struct {
	u16 width;
	u16 height;
	vga_text_cursor_t cursor;
	u8 color;
	u16 *buffer;
	bool initialized;
} vga_text_device_t;

/* VGA text mode API */
kernel_status_t vga_text_init(void);
kernel_status_t vga_text_clear(void);
kernel_status_t vga_text_reset(void);

u8 vga_text_make_color(vga_text_color_t fg, vga_text_color_t bg);
kernel_status_t vga_text_set_color(u8 color);
kernel_status_t vga_text_set_colors(vga_text_color_t fg, vga_text_color_t bg);

kernel_status_t vga_text_set_cursor(u16 x, u16 y);
vga_text_cursor_t vga_text_get_cursor(void);
kernel_status_t vga_text_move_cursor(s16 dx, s16 dy);

kernel_status_t vga_text_putchar_at(char c, u8 color, u16 x, u16 y);
kernel_status_t vga_text_putchar(char c);
kernel_status_t vga_text_write(const char *data, size_t size);
kernel_status_t vga_text_writestring(const char *str);

kernel_status_t vga_text_print_hex(u32 value);
kernel_status_t vga_text_print_dec(u32 value);

kernel_status_t vga_text_scroll_up(u16 lines);
kernel_status_t vga_text_newline(void);

vga_text_device_t *vga_text_get_device(void);

#endif /* DRIVERS_VGA_TEXT_H */