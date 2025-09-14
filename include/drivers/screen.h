#ifndef DRIVERS_SCREEN_H
#define DRIVERS_SCREEN_H

#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/font.h>

void screen_draw_demo(void);
void screen_draw_rect(u16 x, u16 y, u16 width, u16 height, vbe_color_t fill,
                      vbe_color_t border);
void screen_draw_string(u16 x, u16 y, const char *str, vbe_color_t fg,
                        vbe_color_t bg);
void screen_draw_string_centered(u16 y, const char *str, vbe_color_t fg,
                                 vbe_color_t bg);
void screen_draw_string_wrapped(u16 x, u16 y, u16 max_width, const char *str,
                                vbe_color_t fg, vbe_color_t bg);
void screen_clear(vbe_color_t color);
void screen_put_pixel(u16 x, u16 y, vbe_color_t color);
kernel_status_t screen_draw_circle(u16 x, u16 y, u16 radius, vbe_color_t color);

u16 screen_get_width(void);
u16 screen_get_height(void);
u8 screen_get_bpp(void);

void screen_scroll(u16 lines, vbe_color_t bg_color);

#endif // DRIVERS_SCREEN_H