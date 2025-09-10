#include <drivers/serial.h>
#include <kernel/kernel.h>
#include <lib/terminal.h>

void terminal_init(terminal_t *term) {
  term->font = font_get_default();
  if (!term->font) {
    term->max_cols = screen_get_width() / 8;
    term->max_rows = screen_get_height() / 16;
  } else {
    term->max_cols = screen_get_width() / term->font->width;
    term->max_rows = screen_get_height() / term->font->height;
  }

  term->col = 0;
  term->row = 0;
  term->fg_color = VBE_COLOR_WHITE;
  term->bg_color = VBE_COLOR_BLACK;
}

void terminal_putchar(terminal_t *term, char c) {
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
    screen_scroll(term->font->height, term->bg_color);
    term->row = term->max_rows - 1;
  }
}

void terminal_print(terminal_t *term, const char *str) {
  for (const char *p = str; *p != '\0'; ++p) {
    terminal_putchar(term, *p);
  }
}

void terminal_clear(terminal_t *term) {
  screen_clear(term->bg_color);
  term->col = 0;
  term->row = 0;
}

static inline void serial_set_ansi_fg(vbe_color_t color) {
  serial_printf("\x1b[38;2;%u;%u;%um", color.red, color.green, color.blue);
}

static inline void serial_set_ansi_bg(vbe_color_t color) {
  serial_printf("\x1b[48;2;%u;%u;%um", color.red, color.green, color.blue);
}

void terminal_set_fg_color(terminal_t *term, vbe_color_t color) {
  term->fg_color = color;
  serial_set_ansi_fg(color);
}

void terminal_set_bg_color(terminal_t *term, vbe_color_t color) {
  term->bg_color = color;
  serial_set_ansi_bg(color);
}

void terminal_set_bgfg(terminal_t *term, vbe_color_t bg_color,
                       vbe_color_t fg_color) {
  terminal_set_bg_color(term, bg_color);
  terminal_set_fg_color(term, fg_color);
}

int _putchar(char character) {
  terminal_putchar(&g_terminal, character);
  serial_write_char(character);
  return (int)character;
}
