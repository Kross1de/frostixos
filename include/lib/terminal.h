#ifndef LIB_TERMINAL_H
#define LIB_TERMINAL_H

#include <kernel/kernel.h>
#include <drivers/vbe.h>
#include <lib/font.h>
#include <drivers/screen.h>

typedef struct {
	u16 col;
	u16 row;
	u16 max_cols;
	u16 max_rows;
	vbe_color_t fg_color;
	vbe_color_t bg_color;
	const font_t* font;
} terminal_t;

extern terminal_t g_terminal;

void terminal_init(terminal_t* term);
void terminal_putchar(terminal_t* term, char c);
void terminal_print(terminal_t* term, const char* str);
void terminal_clear(terminal_t* term);
void terminal_set_fg_color(terminal_t* term, vbe_color_t color);
void terminal_set_bg_color(terminal_t* term, vbe_color_t color);
void terminal_set_bgfg(terminal_t* term, vbe_color_t bg_color, vbe_color_t fg_color);

#endif // LIB_TERMINAL_H
