#ifndef LIB_TERMINAL_H
#define LIB_TERMINAL_H

#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/ansi_types.h>
#include <lib/font.h>

typedef struct {
  u16 col;
  u16 row;
  u16 max_cols;
  u16 max_rows;
  vbe_color_t fg_color;
  vbe_color_t bg_color;
  const font_t *font;
  ansi_context_t ansi_ctx;
} terminal_t;

extern terminal_t g_terminal;

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

#endif