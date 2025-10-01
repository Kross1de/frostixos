#include <drivers/serial.h>
#include <kernel/kernel.h>
#include <lib/terminal.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static bool cursor_visible = true;

static void handle_ansi_command(terminal_t *term, char cmd);

static vbe_color_t ansi_colors[8] = {
    VBE_COLOR_BLACK, VBE_COLOR_RED,     VBE_COLOR_GREEN, VBE_COLOR_YELLOW,
    VBE_COLOR_BLUE,  VBE_COLOR_MAGENTA, VBE_COLOR_CYAN,  VBE_COLOR_WHITE,
};

static void handle_ansi_command(terminal_t *term, char cmd) {
  switch (cmd) {
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'H':
  case 'J':
  case 'K':
    terminal_toggle_cursor(term);
    break;
  default:
    break;
  }

  if (cmd == 'A') {
    int n = (term->ansi_param_count > 0 ? term->ansi_params[0] : 1);
    term->row = (term->row > n) ? term->row - n : 0;
  } else if (cmd == 'B') {
    int n = (term->ansi_param_count > 0 ? term->ansi_params[0] : 1);
    term->row = MIN(term->max_rows - 1, term->row + n);
  } else if (cmd == 'C') {
    int n = (term->ansi_param_count > 0 ? term->ansi_params[0] : 1);
    term->col = MIN(term->max_cols - 1, term->col + n);
  } else if (cmd == 'D') {
    int n = (term->ansi_param_count > 0 ? term->ansi_params[0] : 1);
    term->col = (term->col > n) ? term->col - n : 0;
  } else if (cmd == 'H') {
    int row = (term->ansi_param_count > 0 ? term->ansi_params[0] : 1);
    int col = (term->ansi_param_count > 1 ? term->ansi_params[1] : 1);
    term->row = MIN(MAX(row - 1, 0), term->max_rows - 1);
    term->col = MIN(MAX(col - 1, 0), term->max_cols - 1);
  } else if (cmd == 'J') {
    int mode = (term->ansi_param_count > 0 ? term->ansi_params[0] : 0);
    u16 font_w = term->font->width;
    u16 font_h = term->font->height;
    u16 screen_w = screen_get_width();
    u16 term_h = term->max_rows * font_h;

    if (mode == 2) {
      vbe_fill_rect(0, 0, screen_w, term_h, term->bg_color);
      term->col = 0;
      term->row = 0;
    } else if (mode == 0) {
      vbe_fill_rect(term->col * font_w, term->row * font_h,
                    (term->max_cols - term->col) * font_w, font_h,
                    term->bg_color);
      if (term->row < term->max_rows - 1) {
        vbe_fill_rect(0, (term->row + 1) * font_h, screen_w,
                      (term->max_rows - term->row - 1) * font_h,
                      term->bg_color);
      }
    } else if (mode == 1) {
      if (term->row > 0) {
        vbe_fill_rect(0, 0, screen_w, term->row * font_h, term->bg_color);
      }
      vbe_fill_rect(0, term->row * font_h, (term->col + 1) * font_w, font_h,
                    term->bg_color);
    }
  } else if (cmd == 'K') {
    int mode = (term->ansi_param_count > 0 ? term->ansi_params[0] : 0);
    u16 font_w = term->font->width;
    u16 font_h = term->font->height;
    u16 lx = 0;
    u16 lw = term->max_cols;

    if (mode == 0) {
      lx = term->col;
      lw = term->max_cols - term->col;
    } else if (mode == 1) {
      lx = 0;
      lw = term->col + 1;
    } else if (mode == 2) {
      lx = 0;
      lw = term->max_cols;
    }
    vbe_fill_rect(lx * font_w, term->row * font_h, lw * font_w, font_h,
                  term->bg_color);
  } else if (cmd == 'm') {
    int i = 0;
    while (i < term->ansi_param_count) {
      int p = term->ansi_params[i];
      if (p == 0) {
        term->fg_color = VBE_COLOR_WHITE;
        term->bg_color = VBE_COLOR_BLACK;
      } else if (p >= 30 && p <= 37) {
        term->fg_color = ansi_colors[p - 30];
      } else if (p >= 40 && p <= 47) {
        term->bg_color = ansi_colors[p - 40];
      } else if (p == 38 || p == 48) {
        bool is_fg = (p == 38);
        if (i + 1 < term->ansi_param_count && term->ansi_params[i + 1] == 2 &&
            i + 4 < term->ansi_param_count) {
          u8 r = (u8)term->ansi_params[i + 2];
          u8 g = (u8)term->ansi_params[i + 3];
          u8 b = (u8)term->ansi_params[i + 4];
          vbe_color_t col = {r, g, b, 255};
          if (is_fg) {
            term->fg_color = col;
          } else {
            term->bg_color = col;
          }
          i += 4;
        } else if (i + 1 < term->ansi_param_count &&
                   term->ansi_params[i + 1] == 5 &&
                   i + 2 < term->ansi_param_count) {
          i += 2;
        }
      }
      i++;
    }
  }

  switch (cmd) {
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'H':
  case 'J':
  case 'K':
    terminal_toggle_cursor(term);
    break;
  default:
    break;
  }
}

void terminal_init(terminal_t *term) {
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
    term->state = ANSI_NORMAL;
    return;
  }

  u16 screen_w = screen_get_width();
  u16 screen_h = screen_get_height();
  term->max_cols = screen_w / term->font->width;
  term->max_rows = (screen_h / term->font->height) - 1;

  term->col = 0;
  term->row = 0;
  term->fg_color = VBE_COLOR_WHITE;
  term->bg_color = VBE_COLOR_BLACK;
  term->state = ANSI_NORMAL;
  term->ansi_param_count = 0;
  term->ansi_private = 0;
  term->ansi_buf_idx = 0;

  terminal_clear(term);
  terminal_draw_cursor(term);
}

void terminal_draw_cursor(terminal_t *term) {
  if (!term || !term->font)
    return;

  u16 x = term->col * term->font->width;
  u16 y = term->row * term->font->height;

  if (cursor_visible) {
    vbe_fill_rect(x, y, term->font->width, term->font->height, term->fg_color);
  } else {
    vbe_fill_rect(x, y, term->font->width, term->font->height, term->bg_color);
  }
}

void terminal_toggle_cursor(terminal_t *term) {
  if (!term)
    return;

  cursor_visible = !cursor_visible;
  terminal_draw_cursor(term);
}

void terminal_putchar(terminal_t *term, char c) {
  if (!term)
    return;

  bool should_toggle = (term->state == ANSI_NORMAL);

  if (should_toggle) {
    terminal_toggle_cursor(term);
  }

  switch (term->state) {
  case ANSI_NORMAL:
    if (c == '\x1b') {
      term->state = ANSI_ESC;
      term->ansi_buf_idx = 0;
      term->ansi_param_count = 0;
      term->ansi_private = 0;
      break;
    }

    if (c == '\n') {
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
      u16 screen_w = screen_get_width();
      u16 term_h = term->max_rows * font_h;
      vbe_blit(0, 0, 0, font_h, screen_w, term_h - font_h);
      vbe_fill_rect(0, (term->max_rows - 1) * font_h, screen_w, font_h,
                    term->bg_color);
      term->row = term->max_rows - 1;
    }
    break;

  case ANSI_ESC:
    if (c == '[') {
      term->state = ANSI_CSI;
      term->ansi_buf_idx = 0;
      break;
    }
    term->state = ANSI_NORMAL;
    break;

  case ANSI_CSI:
    if (isdigit(c)) {
      if (term->ansi_buf_idx < sizeof(term->ansi_buf) - 1) {
        term->ansi_buf[term->ansi_buf_idx++] = c;
      }
    } else if (c == ';') {
      term->ansi_buf[term->ansi_buf_idx] = '\0';
      term->ansi_params[term->ansi_param_count++] =
          (term->ansi_buf_idx > 0 ? atoi(term->ansi_buf) : 0);
      term->ansi_buf_idx = 0;
    } else if (c == '?') {
      term->ansi_private = 1;
    } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
      term->ansi_buf[term->ansi_buf_idx] = '\0';
      if (term->ansi_buf_idx > 0) {
        term->ansi_params[term->ansi_param_count++] = atoi(term->ansi_buf);
      }
      handle_ansi_command(term, c);
      term->state = ANSI_NORMAL;
    } else {
      term->state = ANSI_NORMAL;
    }
    break;
  }

  if (should_toggle) {
    terminal_toggle_cursor(term);
  }
}

void terminal_print(terminal_t *term, const char *str) {
  if (!term || !str)
    return;

  for (const char *p = str; *p != '\0'; ++p) {
    terminal_putchar(term, *p);
  }
}

void terminal_clear(terminal_t *term) {
  if (!term)
    return;

  terminal_toggle_cursor(term);
  u16 font_h = term->font->height;
  vbe_fill_rect(0, 0, screen_get_width(), term->max_rows * font_h,
                term->bg_color);
  term->col = 0;
  term->row = 0;
  terminal_toggle_cursor(term);
}

void terminal_set_fg_color(terminal_t *term, vbe_color_t color) {
  if (!term)
    return;

  terminal_toggle_cursor(term);
  term->fg_color = color;
  serial_set_ansi_fg(color);
  terminal_toggle_cursor(term);
}

void terminal_set_bg_color(terminal_t *term, vbe_color_t color) {
  if (!term)
    return;

  terminal_toggle_cursor(term);
  term->bg_color = color;
  serial_set_ansi_bg(color);
  terminal_toggle_cursor(term);
}

void terminal_set_bgfg(terminal_t *term, vbe_color_t bg_color,
                       vbe_color_t fg_color) {
  terminal_set_bg_color(term, bg_color);
  terminal_set_fg_color(term, fg_color);
}

void terminal_get_cursor(terminal_t *term, int *row, int *col) {
  if (!term || !row || !col)
    return;

  *row = term->row;
  *col = term->col;
}

void terminal_set_cursor(terminal_t *term, int row, int col) {
  if (!term)
    return;

  terminal_toggle_cursor(term);
  term->row = MIN(MAX(row, 0), term->max_rows - 1);
  term->col = MIN(MAX(col, 0), term->max_cols - 1);
  terminal_toggle_cursor(term);
}

int _putchar(char character) {
  terminal_putchar(&g_terminal, character);
  serial_write_char(character);
  return (int)character;
}