/*
 * ANSI/CSI sequence parser and handler.
 */
#include <ctype.h>
#include <drivers/serial.h>
#include <lib/ansi.h>
#include <lib/terminal.h>
#include <printf.h>
#include <string.h>

static vbe_color_t ansi_colors[8] = {
	VBE_COLOR_BLACK, VBE_COLOR_RED, VBE_COLOR_GREEN, VBE_COLOR_YELLOW,
	VBE_COLOR_BLUE,  VBE_COLOR_MAGENTA, VBE_COLOR_CYAN,  VBE_COLOR_WHITE,
};

static void handle_ansi_command(ansi_context_t *ctx, terminal_t *term, char cmd)
{
	/* Handle movement & mode commands first (these update cursor). */
	if (cmd == 'A' || cmd == 'B' || cmd == 'C' || cmd == 'D' || cmd == 'H') {
		int n = ctx->ansi_param_count > 0 ? ctx->ansi_params[0] : 1;
		if (cmd == 'A')
			term->row = (term->row > n) ? term->row - n : 0;
		else if (cmd == 'B')
			term->row = MIN(term->max_rows - 1, term->row + n);
		else if (cmd == 'C')
			term->col = MIN(term->max_cols - 1, term->col + n);
		else if (cmd == 'D')
			term->col = (term->col > n) ? term->col - n : 0;
		else if (cmd == 'H') {
			int row = ctx->ansi_param_count > 0 ? ctx->ansi_params[0] : 1;
			int col = ctx->ansi_param_count > 1 ? ctx->ansi_params[1] : 1;
			term->row = MIN(MAX(row - 1, 0), term->max_rows - 1);
			term->col = MIN(MAX(col - 1, 0), term->max_cols - 1);
		}
	}

	/* Erase / clear commands */
	if (cmd == 'J') {
		int mode = ctx->ansi_param_count > 0 ? ctx->ansi_params[0] : 0;
		u16 font_w = term->font->width;
		u16 font_h = term->font->height;
		u16 screen_w = vbe_get_width();
		u16 term_h = term->max_rows * font_h;

		if (mode == 2) {
			vbe_fill_rect(0, 0, screen_w, term_h, term->bg_color);
			term->col = term->row = 0;
		} else {
			/* partial clears implemented conservatively */
			vbe_fill_rect(term->col * font_w, term->row * font_h,
				      (term->max_cols - term->col) * font_w,
				      font_h, term->bg_color);
		}
	}

	if (cmd == 'K') {
		int mode = ctx->ansi_param_count > 0 ? ctx->ansi_params[0] : 0;
		u16 font_w = term->font->width;
		u16 lx = 0, lw = term->max_cols;

		if (mode == 0) { lx = term->col; lw = term->max_cols - term->col; }
		else if (mode == 1) { lx = 0; lw = term->col + 1; }
		else if (mode == 2) { lx = 0; lw = term->max_cols; }

		vbe_fill_rect(lx * font_w, term->row * term->font->height, lw * font_w,
			      term->font->height, term->bg_color);
	}

	/* SGR: select graphic rendition (colors) */
	if (cmd == 'm') {
		int i = 0;
		while (i < ctx->ansi_param_count) {
			int p = ctx->ansi_params[i];
			if (p == 0) {
				term->fg_color = VBE_COLOR_WHITE;
				term->bg_color = VBE_COLOR_BLACK;
			} else if (p >= 30 && p <= 37) {
				term->fg_color = ansi_colors[p - 30];
			} else if (p >= 40 && p <= 47) {
				term->bg_color = ansi_colors[p - 40];
			} else if (p == 38 || p == 48) {
				bool is_fg = (p == 38);
				/* 24-bit color: 38;2;r;g;b */
				if (i + 1 < ctx->ansi_param_count && ctx->ansi_params[i+1] == 2 &&
				    i + 4 < ctx->ansi_param_count) {
					u8 r = (u8)ctx->ansi_params[i+2];
					u8 g = (u8)ctx->ansi_params[i+3];
					u8 b = (u8)ctx->ansi_params[i+4];
					vbe_color_t col = { r, g, b, 255 };
					if (is_fg) {
						term->fg_color = col;
						serial_set_ansi_fg(col);
					} else {
						term->bg_color = col;
						serial_set_ansi_bg(col);
					}
					i += 4;
				} else {
					/* skip unknown forms */
				}
			}
			i++;
		}
	}

	/* Save/restore cursor */
	if (cmd == 's') {
		ctx->saved_row = term->row;
		ctx->saved_col = term->col;
	} else if (cmd == 'u') {
		term->row = ctx->saved_row;
		term->col = ctx->saved_col;
	}

	/* Device status report, cursor position */
	if (cmd == 'n' && ctx->ansi_param_count > 0 && ctx->ansi_params[0] == 6) {
		char resp[32];
		snprintf(resp, sizeof(resp), "\x1b[%d;%dR", term->row + 1, term->col + 1);
		serial_printf("%s", resp);
	}
}

void ansi_init(ansi_context_t *ctx)
{
	if (!ctx)
		return;

	ctx->state = ANSI_NORMAL;
	ctx->ansi_private = 0;
	ctx->ansi_buf_idx = 0;
	ctx->ansi_param_count = 0;
	ctx->saved_row = ctx->saved_col = 0;
	ctx->cursor_enabled = true;
}

void ansi_process_char(ansi_context_t *ctx, terminal_t *term, char c)
{
	if (!ctx || !term)
		return;

	switch (ctx->state) {
	case ANSI_NORMAL:
		if (c == '\x1b') {
			ctx->state = ANSI_ESC;
			ctx->ansi_buf_idx = 0;
			ctx->ansi_param_count = 0;
			ctx->ansi_private = 0;
		}
		break;

	case ANSI_ESC:
		if (c == '[') {
			ctx->state = ANSI_CSI;
			ctx->ansi_buf_idx = 0;
		} else {
			ctx->state = ANSI_NORMAL;
		}
		break;

	case ANSI_CSI:
		if (isdigit((unsigned char)c)) {
			if (ctx->ansi_buf_idx < (int)(sizeof(ctx->ansi_buf) - 1))
				ctx->ansi_buf[ctx->ansi_buf_idx++] = c;
		} else if (c == ';') {
			ctx->ansi_buf[ctx->ansi_buf_idx] = '\0';
			ctx->ansi_params[ctx->ansi_param_count++] = (ctx->ansi_buf_idx > 0) ? atoi(ctx->ansi_buf) : 0;
			ctx->ansi_buf_idx = 0;
		} else if (c == '?') {
			ctx->ansi_private = 1;
		} else if (isalpha((unsigned char)c)) {
			ctx->ansi_buf[ctx->ansi_buf_idx] = '\0';
			if (ctx->ansi_buf_idx > 0)
				ctx->ansi_params[ctx->ansi_param_count++] = atoi(ctx->ansi_buf);
			handle_ansi_command(ctx, term, c);
			ctx->state = ANSI_NORMAL;
		} else {
			ctx->state = ANSI_NORMAL;
		}
		break;
	}
}