#ifndef LIB_FONT_H
#define LIB_FONT_H

#include <drivers/vbe.h>
#include <kernel/kernel.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define FONT_CHARS 256

/*
 * Bitmap font representation. Each character is FONT_HEIGHT bytes,
 * where each byte represents a row of 8 pixels (MSB/LSB layout depends
 * on rendering code).
 */
typedef struct {
	u8 width;				/* Character width in pixels */
	u8 height;				/* Character height in pixels */
	u8 data[FONT_CHARS][FONT_HEIGHT];	/* Bitmap data for each glyph */
} font_t;

/*
 * Text rendering context tracks current position, colors and the
 * selected font to use for output.
 */
typedef struct {
	u16 x;			     /* Current X position (pixels) */
	u16 y;			     /* Current Y position (pixels) */
	vbe_color_t fg_color;	     /* Foreground color */
	vbe_color_t bg_color;	     /* Background color */
	const font_t *font;	     /* Currently active font */
} text_context_t;

/* Font subsystem API */
kernel_status_t font_init(void);
const font_t *font_get_default(void);
kernel_status_t font_render_char(char c, u16 x, u16 y, vbe_color_t fg_color,
				 vbe_color_t bg_color, const font_t *font);
kernel_status_t font_render_string(const char *str, u16 x, u16 y,
				   vbe_color_t fg_color, vbe_color_t bg_color,
				   const font_t *font);

/* Text context helpers */
void text_context_init(text_context_t *ctx, u16 x, u16 y, vbe_color_t fg_color,
		       vbe_color_t bg_color);
kernel_status_t text_context_putchar(text_context_t *ctx, char c);
kernel_status_t text_context_print(text_context_t *ctx, const char *str);
void text_context_newline(text_context_t *ctx);
void text_context_set_position(text_context_t *ctx, u16 x, u16 y);
void text_context_set_colors(text_context_t *ctx, vbe_color_t fg_color,
			     vbe_color_t bg_color);

#endif /* LIB_FONT_H */