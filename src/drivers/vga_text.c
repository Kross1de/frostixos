#include <drivers/vga_text.h>
#include <kernel/kernel.h>
#include <stdarg.h>
#include <string.h>

#define VGA_MEMORY 0xB8000

static vga_text_device_t g_vga_text_device = {0};

static inline u16 vga_make_entry(char c, u8 color) {
  return (u16)c | ((u16)color << 8);
}

static inline size_t vga_index(u16 x, u16 y) { return y * VGA_TEXT_WIDTH + x; }

static void vga_text_scroll_internal(void) {
  for (u16 y = 0; y < VGA_TEXT_HEIGHT - 1; y++) {
    for (u16 x = 0; x < VGA_TEXT_WIDTH; x++) {
      size_t src_index = vga_index(x, y + 1);
      size_t dest_index = vga_index(x, y);
      g_vga_text_device.buffer[dest_index] =
          g_vga_text_device.buffer[src_index];
    }
  }
  for (u16 x = 0; x < VGA_TEXT_WIDTH; x++) {
    size_t index = vga_index(x, VGA_TEXT_HEIGHT - 1);
    g_vga_text_device.buffer[index] =
        vga_make_entry(' ', g_vga_text_device.color);
  }
}

kernel_status_t vga_text_init(void) {
  g_vga_text_device.width = VGA_TEXT_WIDTH;
  g_vga_text_device.height = VGA_TEXT_HEIGHT;
  g_vga_text_device.cursor.x = 0;
  g_vga_text_device.cursor.y = 0;
  g_vga_text_device.color =
      vga_text_make_color(VGA_TEXT_COLOR_LIGHT_GREY, VGA_TEXT_COLOR_BLACK);
  g_vga_text_device.buffer = (u16 *)VGA_MEMORY;
  g_vga_text_device.initialized = true;
  return vga_text_clear();
}

kernel_status_t vga_text_clear(void) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  for (size_t i = 0; i < VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT; i++) {
    g_vga_text_device.buffer[i] = vga_make_entry(' ', g_vga_text_device.color);
  }
  g_vga_text_device.cursor.x = 0;
  g_vga_text_device.cursor.y = 0;
  return KERNEL_OK;
}

kernel_status_t vga_text_reset(void) {
  g_vga_text_device.color =
      vga_text_make_color(VGA_TEXT_COLOR_LIGHT_GREY, VGA_TEXT_COLOR_BLACK);
  return vga_text_clear();
}

u8 vga_text_make_color(vga_text_color_t fg, vga_text_color_t bg) {
  return (u8)fg | ((u8)bg << 4);
}

kernel_status_t vga_text_set_color(u8 color) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  g_vga_text_device.color = color;
  return KERNEL_OK;
}

kernel_status_t vga_text_set_colors(vga_text_color_t fg, vga_text_color_t bg) {
  return vga_text_set_color(vga_text_make_color(fg, bg));
}

kernel_status_t vga_text_set_cursor(u16 x, u16 y) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  if (x >= VGA_TEXT_WIDTH || y >= VGA_TEXT_HEIGHT) {
    return KERNEL_INVALID_PARAM;
  }
  g_vga_text_device.cursor.x = x;
  g_vga_text_device.cursor.y = y;
  return KERNEL_OK;
}

vga_text_cursor_t vga_text_get_cursor(void) { return g_vga_text_device.cursor; }

kernel_status_t vga_text_move_cursor(s16 dx, s16 dy) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  s32 new_x = (s32)g_vga_text_device.cursor.x + dx;
  s32 new_y = (s32)g_vga_text_device.cursor.y + dy;
  if (new_x < 0)
    new_x = 0;
  if (new_x >= VGA_TEXT_WIDTH)
    new_x = VGA_TEXT_WIDTH - 1;
  if (new_y < 0)
    new_y = 0;
  if (new_y >= VGA_TEXT_HEIGHT)
    new_y = VGA_TEXT_HEIGHT - 1;
  return vga_text_set_cursor((u16)new_x, (u16)new_y);
}

kernel_status_t vga_text_putchar_at(char c, u8 color, u16 x, u16 y) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  if (x >= VGA_TEXT_WIDTH || y >= VGA_TEXT_HEIGHT) {
    return KERNEL_INVALID_PARAM;
  }
  size_t index = vga_index(x, y);
  g_vga_text_device.buffer[index] = vga_make_entry(c, color);
  return KERNEL_OK;
}

kernel_status_t vga_text_newline(void) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  g_vga_text_device.cursor.x = 0;
  if (++g_vga_text_device.cursor.y >= VGA_TEXT_HEIGHT) {
    g_vga_text_device.cursor.y = VGA_TEXT_HEIGHT - 1;
    vga_text_scroll_internal();
  }
  return KERNEL_OK;
}

kernel_status_t vga_text_putchar(char c) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  if (c == '\n') {
    return vga_text_newline();
  } else if (c == '\r') {
    g_vga_text_device.cursor.x = 0;
    return KERNEL_OK;
  } else if (c == '\t') {
    u16 spaces = 4 - (g_vga_text_device.cursor.x % 4);
    for (u16 i = 0; i < spaces; i++) {
      kernel_status_t status = vga_text_putchar(' ');
      if (status != KERNEL_OK) {
        return status;
      }
    }
    return KERNEL_OK;
  } else if (c == '\b') {
    if (g_vga_text_device.cursor.x > 0) {
      g_vga_text_device.cursor.x--;
      vga_text_putchar_at(' ', g_vga_text_device.color,
                          g_vga_text_device.cursor.x,
                          g_vga_text_device.cursor.y);
    }
    return KERNEL_OK;
  }
  vga_text_putchar_at(c, g_vga_text_device.color, g_vga_text_device.cursor.x,
                      g_vga_text_device.cursor.y);
  if (++g_vga_text_device.cursor.x >= VGA_TEXT_WIDTH) {
    return vga_text_newline();
  }
  return KERNEL_OK;
}

kernel_status_t vga_text_write(const char *data, size_t size) {
  if (!g_vga_text_device.initialized || !data) {
    return KERNEL_INVALID_PARAM;
  }
  for (size_t i = 0; i < size; i++) {
    kernel_status_t status = vga_text_putchar(data[i]);
    if (status != KERNEL_OK) {
      return status;
    }
  }
  return KERNEL_OK;
}

kernel_status_t vga_text_writestring(const char *str) {
  if (!str) {
    return KERNEL_INVALID_PARAM;
  }
  return vga_text_write(str, strlen(str));
}

kernel_status_t vga_text_print_hex(u32 value) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  char hex_chars[] = "0123456789ABCDEF";
  char buffer[11];
  buffer[0] = '0';
  buffer[1] = 'x';
  buffer[10] = '\0';
  for (int i = 9; i >= 2; i--) {
    buffer[i] = hex_chars[value & 0xF];
    value >>= 4;
  }
  return vga_text_writestring(buffer);
}

kernel_status_t vga_text_print_dec(u32 value) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  if (value == 0) {
    return vga_text_putchar('0');
  }
  char buffer[12];
  int pos = 11;
  buffer[pos] = '\0';
  while (value > 0 && pos > 0) {
    buffer[--pos] = '0' + (value % 10);
    value /= 10;
  }
  return vga_text_writestring(&buffer[pos]);
}

kernel_status_t vga_text_scroll_up(u16 lines) {
  if (!g_vga_text_device.initialized) {
    return KERNEL_ERROR;
  }
  for (u16 i = 0; i < lines; i++) {
    vga_text_scroll_internal();
  }
  return KERNEL_OK;
}

vga_text_device_t *vga_text_get_device(void) {
  return g_vga_text_device.initialized ? &g_vga_text_device : NULL;
}
