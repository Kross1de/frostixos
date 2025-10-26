/*
 * Terminal abstraction.
 */
#include <drivers/serial.h>
#include <kernel/kernel.h>
#include <lib/ansi.h>
#include <lib/terminal.h>
#include <string.h>

static bool cursor_visible = true;

void terminal_init(terminal_t *term)
{
	if (!term)
		return;

	term->font = font_get_default();
	if (!term->font) {
		term->max_cols = 80;
		term->max_rows = 25;
		term->col = 0;
		term->row = 0;
		term->fg_color = VBE_COLOR_WHITE;
		term->bg_color = VBE_COLOR_BLACK;
		ansi_init(&term->ansi_ctx);
		return;
	}

	u16 screen_w = vbe_get_width();
	u16 screen_h = vbe_get_height();
	term->max_cols = screen_w / term->font->width;
	term->max_rows = (screen_h / term->font->height) - 1;

	term->col = 0;
	term->row = 0;
	term->fg_color = VBE_COLOR_WHITE;
	term->bg_color = VBE_COLOR_BLACK;
	ansi_init(&term->ansi_ctx);

	terminal_clear(term);
	terminal_draw_cursor(term);
}

void terminal_draw_cursor(terminal_t *term)
{
	if (!term || !term->font) return;
	if (!term->ansi_ctx.cursor_enabled) return;

	u16 x = term->col * term->font->width;
	u16 y = term->row * term->font->height;

	if (cursor_visible)
		vbe_fill_rect(x, y, term->font->width, term->font->height, term->fg_color);
	else
		vbe_fill_rect(x, y, term->font->width, term->font->height, term->bg_color);
}

void terminal_toggle_cursor(terminal_t *term)
{
	if (!term) return;
	if (!term->ansi_ctx.cursor_enabled) return;

	cursor_visible = !cursor_visible;
	terminal_draw_cursor(term);
}

void terminal_putchar(terminal_t *term, char c)
{
	if (!term) return;

	bool should_toggle = (term->ansi_ctx.state == ANSI_NORMAL);

	if (should_toggle)
		terminal_toggle_cursor(term);

	if (term->ansi_ctx.state == ANSI_NORMAL) {
		if (c == '\x1b') {
			ansi_process_char(&term->ansi_ctx, term, c);
		} else if (c == '\n') {
			term->col = 0;
			term->row++;
		} else if (c == '\r') {
			term->col = 0;
		} else if (c == '\b') {
			if (term->col > 0) {
				term->col--;
			} else if (term->row > 0) {
				term->row--;
				term->col = term->max_cols - 1;
			}
			u16 x = term->col * term->font->width;
			u16 y = term->row * term->font->height;
			font_render_char(' ', x, y, term->fg_color, term->bg_color, term->font);
		} else if (c == '\t') {
			term->col = (term->col + 8) & ~7;
			if (term->col >= term->max_cols) {
				term->col = 0;
				term->row++;
			}
		} else if (c >= ' ') {
			u16 x = term->col * term->font->width;
			u16 y = term->row * term->font->height;
			font_render_char(c, x, y, term->fg_color, term->bg_color, term->font);
			term->col++;
			if (term->col >= term->max_cols) {
				term->col = 0;
				term->row++;
			}
		}

		if (term->row >= term->max_rows) {
			u16 font_h = term->font->height;
			u16 screen_w = vbe_get_width();
			u16 term_h = term->max_rows * font_h;
			vbe_blit(0, 0, 0, font_h, screen_w, term_h - font_h);
			vbe_fill_rect(0, (term->max_rows - 1) * font_h, screen_w, font_h, term->bg_color);
			term->row = term->max_rows - 1;
		}
	} else {
		ansi_process_char(&term->ansi_ctx, term, c);
	}

	if (should_toggle)
		terminal_toggle_cursor(term);
}

void terminal_print(terminal_t *term, const char *str)
{
	if (!term || !str) return;
	for (const char *p = str; *p; ++p)
		terminal_putchar(term, *p);
}

void terminal_clear(terminal_t *term)
{
	if (!term) return;
	terminal_toggle_cursor(term);
	u16 font_h = term->font->height;
	vbe_fill_rect(0, 0, vbe_get_width(), term->max_rows * font_h, term->bg_color);
	term->col = 0;
	term->row = 0;
	terminal_toggle_cursor(term);
}

void terminal_set_fg_color(terminal_t *term, vbe_color_t color)
{
	if (!term) return;
	terminal_toggle_cursor(term);
	term->fg_color = color;
	serial_set_ansi_fg(color);
	terminal_toggle_cursor(term);
}

void terminal_set_bg_color(terminal_t *term, vbe_color_t color)
{
	if (!term) return;
	terminal_toggle_cursor(term);
	term->bg_color = color;
	serial_set_ansi_bg(color);
	terminal_toggle_cursor(term);
}

void terminal_set_bgfg(terminal_t *term, vbe_color_t bg_color, vbe_color_t fg_color)
{
	terminal_set_bg_color(term, bg_color);
	terminal_set_fg_color(term, fg_color);
}

void terminal_get_cursor(terminal_t *term, int *row, int *col)
{
	if (!term || !row || !col) return;
	*row = term->row;
	*col = term->col;
}

void terminal_set_cursor(terminal_t *term, int row, int col)
{
	if (!term) return;
	terminal_toggle_cursor(term);
	term->row = MIN(MAX(row, 0), term->max_rows - 1);
	term->col = MIN(MAX(col, 0), term->max_cols - 1);
	terminal_toggle_cursor(term);
}

/* Kernel hook for printf */
int _putchar(char character)
{
	terminal_putchar(&g_terminal, character);
	serial_write_char(character);
	return (int)character;
}