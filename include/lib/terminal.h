#ifndef LIB_TERMINAL_H
#define LIB_TERMINAL_H

#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/ansi_types.h>
#include <lib/font.h>

/*
 * Terminal abstraction built on top of the text rendering/font/vbe
 * primitives. Maintains cursor position, dimensions and colors.
 */
typedef struct {
	u16 col;		   /* current cursor column (char cells) */
	u16 row;		   /* current cursor row (char cells) */
	u16 max_cols;		   /* maximum columns */
	u16 max_rows;		   /* maximum rows */
	vbe_color_t fg_color;	   /* foreground color */
	vbe_color_t bg_color;	   /* background color */
	const font_t *font;	   /* current font pointer */
	ansi_context_t ansi_ctx;   /* ANSI parser state */
} terminal_t;

/* Global terminal instance used by core code (if present). */
extern terminal_t g_terminal;

/* Terminal API */
void terminal_init(terminal_t *term);
void terminal_draw_cursor(terminal_t *term);
void terminal_toggle_cursor(terminal_t *term);
void terminal_putchar(terminal_t *term, char c);
void terminal_print(terminal_t *term, const char *str);
void terminal_clear(terminal_t *term);
void terminal_set_fg_color(terminal_t *term, vbe_color_t color);
void terminal_set_bg_color(terminal_t *term, vbe_color_t color);
void terminal_set_bgfg(terminal_t *term, vbe_color_t bg_color,
		       vbe_color_t fg_color);
void terminal_get_cursor(terminal_t *term, int *row, int *col);
void terminal_set_cursor(terminal_t *term, int row, int col);

#endif /* LIB_TERMINAL_H */