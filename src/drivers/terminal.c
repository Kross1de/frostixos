#include <drivers/terminal.h>
#include <kernel/kernel.h>

void terminal_init(terminal_t* term) {
    if (!term) {
        return;
    }

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

void terminal_putchar(terminal_t* term, char c) {
    if (!term || !term->font) {
        return;
    }

    if (c == '\n') {
        term->col = 0;
        term->row++;
    } else if (c == '\r') {
        term->col = 0;
    } else {
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

void terminal_print(terminal_t* term, const char* str) {
    if (!term || !str) {
        return;
    }

    for (const char* p = str; *p != '\0'; ++p) {
        terminal_putchar(term, *p);
    }
}

void terminal_clear(terminal_t* term) {
    if (!term) {
        return;
    }

    screen_clear(term->bg_color);
    term->col = 0;
    term->row = 0;
}
