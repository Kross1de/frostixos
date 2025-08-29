#ifndef LIB_FONT_H
#define LIB_FONT_H

#include <kernel/kernel.h>
#include <drivers/vbe.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16
#define FONT_CHARS  256

typedef struct {
    u8 width;                         // Character width in pixels
    u8 height;                        // Character height in pixels
    u8 data[FONT_CHARS][FONT_HEIGHT]; // Bitmap data for each character
} font_t;

typedef struct {
    u16 x;                // Current X position
    u16 y;                // Current Y position
    vbe_color_t fg_color; // Foreground color
    vbe_color_t bg_color; // Background color
    const font_t* font;   // Current font
} text_context_t;

kernel_status_t font_init(void);
const font_t* font_get_default(void);
kernel_status_t font_render_char(char c, u16 x, u16 y, vbe_color_t fg_color, vbe_color_t bg_color, const font_t* font);
kernel_status_t font_render_string(const char* str, u16 x, u16 y, vbe_color_t fg_color, vbe_color_t bg_color, const font_t* font);

void text_context_init(text_context_t* ctx, u16 x, u16 y, vbe_color_t fg_color, vbe_color_t bg_color);
kernel_status_t text_context_putchar(text_context_t* ctx, char c);
kernel_status_t text_context_print(text_context_t* ctx, const char* str);
void text_context_newline(text_context_t* ctx);
void text_context_set_position(text_context_t* ctx, u16 x, u16 y);
void text_context_set_colors(text_context_t* ctx, vbe_color_t fg_color, vbe_color_t bg_color);

#endif // LIB_FONT_H
