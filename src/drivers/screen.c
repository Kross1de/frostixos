#include <drivers/screen.h>
#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/font.h>

void screen_draw_rect(u16 x, u16 y, u16 width, u16 height, vbe_color_t fill,
                      vbe_color_t border) {
  vbe_fill_rect(x, y, width, height, fill);
  vbe_draw_rect(x, y, width, height, border);
}

void screen_draw_string(u16 x, u16 y, const char *str, vbe_color_t fg,
                        vbe_color_t bg) {
  font_render_string(str, x, y, fg, bg, font_get_default());
}

void screen_draw_string_centered(u16 y, const char *str, vbe_color_t fg,
                                 vbe_color_t bg) {
  const font_t *font = font_get_default();
  size_t len = strlen(str);
  u16 screen_width = screen_get_width();
  u16 str_width = len * font->width;
  u16 x = (screen_width > str_width) ? (screen_width - str_width) / 2 : 0;
  screen_draw_string(x, y, str, fg, bg);
}

void screen_draw_demo(void) {
  screen_draw_rect(400, 100, 200, 100, VBE_COLOR_DARK_GRAY, VBE_COLOR_WHITE);
  screen_draw_string(420, 120, "FrostixOS VBE Demo", VBE_COLOR_WHITE,
                     VBE_COLOR_DARK_GRAY);

  vbe_draw_line(400, 210, 600, 210, VBE_COLOR_RED);
  vbe_draw_line(400, 100, 600, 200, VBE_COLOR_GREEN);
  vbe_draw_line(400, 200, 600, 100, VBE_COLOR_BLUE);

  for (int i = 0; i < 32; ++i) {
    vbe_put_pixel(400 + i, 230, VBE_COLOR_YELLOW);
    vbe_put_pixel(400 + i, 232, VBE_COLOR_MAGENTA);
    vbe_put_pixel(400 + i, 234, VBE_COLOR_CYAN);
  }
}

void screen_clear(vbe_color_t color) { vbe_clear_screen(color); }

void screen_put_pixel(u16 x, u16 y, vbe_color_t color) {
  vbe_put_pixel(x, y, color);
}

u16 screen_get_width(void) {
  vbe_device_t *dev = vbe_get_device();
  return dev ? dev->width : 0;
}
u16 screen_get_height(void) {
  vbe_device_t *dev = vbe_get_device();
  return dev ? dev->height : 0;
}
u8 screen_get_bpp(void) {
  vbe_device_t *dev = vbe_get_device();
  return dev ? dev->bpp : 0;
}

void screen_scroll(u16 lines, vbe_color_t bg_color) {
  vbe_device_t *dev = vbe_get_device();
  if (!dev || lines == 0 || lines >= dev->height)
    return;
  u8 *fb = vbe_get_framebuffer();
  size_t row_bytes = dev->pitch * lines;
  size_t total_bytes = dev->pitch * dev->height;
  memmove(fb, fb + row_bytes, total_bytes - row_bytes);
  vbe_fill_rect(0, dev->height - lines, dev->width, lines, bg_color);
}
