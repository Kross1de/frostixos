#include <drivers/serial.h>
#include <kernel/kernel.h>
#include <lib/terminal.h>

static bool cursor_visible = true;

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
  cursor_visible = true;
  terminal_draw_cursor(term);
}

void terminal_draw_cursor(terminal_t *term) {
  if (!term->font)
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
  cursor_visible = !cursor_visible;
  terminal_draw_cursor(term);
}

void terminal_putchar(terminal_t *term, char c) {
  terminal_toggle_cursor(term);

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

  terminal_toggle_cursor(term);
}

void terminal_print(terminal_t *term, const char *str) {
  for (const char *p = str; *p != '\0'; ++p) {
    terminal_putchar(term, *p);
  }
}

void terminal_clear(terminal_t *term) {
  terminal_toggle_cursor(term);
  screen_clear(term->bg_color);
  term->col = 0;
  term->row = 0;
  terminal_toggle_cursor(term);
}

static inline void serial_set_ansi_fg(vbe_color_t color) {
  serial_printf("\x1b[38;2;%u;%u;%um", color.red, color.green, color.blue);
}

static inline void serial_set_ansi_bg(vbe_color_t color) {
  serial_printf("\x1b[48;2;%u;%u;%um", color.red, color.green, color.blue);
}

void terminal_set_fg_color(terminal_t *term, vbe_color_t color) {
  terminal_toggle_cursor(term);
  term->fg_color = color;
  serial_set_ansi_fg(color);
  terminal_toggle_cursor(term);
}

void terminal_set_bg_color(terminal_t *term, vbe_color_t color) {
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

int _putchar(char character) {
  terminal_putchar(&g_terminal, character);
  serial_write_char(character);
  return (int)character;
}
