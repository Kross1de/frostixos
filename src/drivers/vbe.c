#include <arch/i386/multiboot.h>
#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/font.h>
#include <lib/terminal.h>
#include <misc/logger.h>
#include <mm/vmm.h>
#include <printf.h>
#include <string.h>

extern u32 _multiboot_info_ptr;

static vbe_device_t g_device = {0};

#define VBE_WRITE32(addr, val) (*(volatile u32 *)(addr) = (val))

/* Replicate color for word fill based on bpp */
static u32 vbe_replicate_pixel(u32 pixel, u8 bpp) {
  if (bpp == 32)
    return pixel;
  if (bpp == 16 || bpp == 15)
    return (pixel << 16) | pixel;
  if (bpp == 8)
    return (pixel << 24) | (pixel << 16) | (pixel << 8) | pixel;
  if (bpp == 24) {
    /* For 24bpp, replicate as RGBRGBRG */
    u32 rgb = pixel & 0xFFFFFF;
    return (rgb << 8) | (rgb >> 16); /* Approximate for u32 write */
  }
  return pixel;
}

kernel_status_t vbe_init(void) {
  multiboot_info_t *mbi = (multiboot_info_t *)_multiboot_info_ptr;
  if (!(mbi->flags & (1 << 11))) {
    return KERNEL_ERROR;
  }

  memcpy(&g_device.control_info, (void *)mbi->vbe_control_info,
         sizeof(vbe_control_info_t));
  memcpy(&g_device.mode_info, (void *)mbi->vbe_mode_info,
         sizeof(vbe_mode_info_t));

  g_device.current_mode = mbi->vbe_mode;
  g_device.framebuffer_addr = g_device.mode_info.phys_base_ptr;
  g_device.width = g_device.mode_info.x_resolution;
  g_device.height = g_device.mode_info.y_resolution;
  g_device.bpp = g_device.mode_info.bits_per_pixel;
  g_device.pitch = g_device.mode_info.bytes_per_scanline;
  if (g_device.mode_info.lin_bytes_per_scan_line) {
    g_device.pitch = g_device.mode_info.lin_bytes_per_scan_line;
  }
  g_device.memory_model = g_device.mode_info.memory_model;

  g_device.red_mask_size = g_device.mode_info.red_mask_size;
  g_device.red_field_position = g_device.mode_info.red_field_position;
  g_device.green_mask_size = g_device.mode_info.green_mask_size;
  g_device.green_field_position = g_device.mode_info.green_field_position;
  g_device.blue_mask_size = g_device.mode_info.blue_mask_size;
  g_device.blue_field_position = g_device.mode_info.blue_field_position;
  if (g_device.mode_info.lin_red_mask_size) {
    g_device.red_mask_size = g_device.mode_info.lin_red_mask_size;
    g_device.red_field_position = g_device.mode_info.lin_red_field_position;
    g_device.green_mask_size = g_device.mode_info.lin_green_mask_size;
    g_device.green_field_position = g_device.mode_info.lin_green_field_position;
    g_device.blue_mask_size = g_device.mode_info.lin_blue_mask_size;
    g_device.blue_field_position = g_device.mode_info.lin_blue_field_position;
  }

  g_device.framebuffer_size = (u32)g_device.height * g_device.pitch;
  g_device.linear_supported =
      (g_device.mode_info.mode_attributes & VBE_MODE_ATTR_LINEAR) != 0;
  g_device.initialized = true;

  return KERNEL_OK;
}

vbe_device_t *vbe_get_device(void) { return &g_device; }

bool vbe_is_available(void) { return g_device.initialized; }

kernel_status_t vbe_set_mode(u16 mode) {
  UNUSED(mode);
  return KERNEL_NOT_IMPLEMENTED;
}

kernel_status_t vbe_get_mode_info(u16 mode, vbe_mode_info_t *mode_info) {
  if (mode != g_device.current_mode || !mode_info) {
    return KERNEL_NOT_IMPLEMENTED;
  }
  memcpy(mode_info, &g_device.mode_info, sizeof(vbe_mode_info_t));
  return KERNEL_OK;
}

kernel_status_t vbe_map_framebuffer(void) { return KERNEL_OK; }

u8 *vbe_get_framebuffer(void) { return (u8 *)g_device.framebuffer_addr; }

u32 vbe_color_to_pixel(vbe_color_t color) {
  u32 r = (u32)color.red * ((1 << g_device.red_mask_size) - 1) / 255;
  u32 g = (u32)color.green * ((1 << g_device.green_mask_size) - 1) / 255;
  u32 b = (u32)color.blue * ((1 << g_device.blue_mask_size) - 1) / 255;
  u32 a =
      (u32)color.alpha * ((1 << g_device.mode_info.rsvd_mask_size) - 1) / 255;

  return (r << g_device.red_field_position) |
         (g << g_device.green_field_position) |
         (b << g_device.blue_field_position) |
         (a << g_device.mode_info.rsvd_field_position);
}

vbe_color_t vbe_pixel_to_color(u32 pixel) {
  vbe_color_t color;
  u32 r_mask = ((1 << g_device.red_mask_size) - 1);
  u32 g_mask = ((1 << g_device.green_mask_size) - 1);
  u32 b_mask = ((1 << g_device.blue_mask_size) - 1);
  u32 a_mask = ((1 << g_device.mode_info.rsvd_mask_size) - 1);

  color.red = ((pixel >> g_device.red_field_position) & r_mask) * 255 / r_mask;
  color.green =
      ((pixel >> g_device.green_field_position) & g_mask) * 255 / g_mask;
  color.blue =
      ((pixel >> g_device.blue_field_position) & b_mask) * 255 / b_mask;
  color.alpha = ((pixel >> g_device.mode_info.rsvd_field_position) & a_mask) *
                255 / a_mask;
  return color;
}

kernel_status_t vbe_put_pixel(u16 x, u16 y, vbe_color_t color) {
  if (!g_device.initialized || x >= g_device.width || y >= g_device.height) {
    return KERNEL_ERROR;
  }
  u8 *fb = vbe_get_framebuffer();
  u32 offset = y * g_device.pitch + x * (g_device.bpp / 8);
  u32 pixel = vbe_color_to_pixel(color);
  switch (g_device.bpp) {
  case 8:
    *(u8 *)(fb + offset) = (u8)pixel;
    break;
  case 15:
  case 16:
    *(u16 *)(fb + offset) = (u16)pixel;
    break;
  case 24:
    *(u8 *)(fb + offset) = (u8)(pixel & 0xFF);
    *(u8 *)(fb + offset + 1) = (u8)((pixel >> 8) & 0xFF);
    *(u8 *)(fb + offset + 2) = (u8)((pixel >> 16) & 0xFF);
    break;
  case 32:
    *(u32 *)(fb + offset) = pixel;
    break;
  default:
    return KERNEL_ERROR;
  }
  return KERNEL_OK;
}

vbe_color_t vbe_get_pixel(u16 x, u16 y) {
  vbe_color_t color = {0, 0, 0, 0};
  if (!g_device.initialized || x >= g_device.width || y >= g_device.height) {
    return color;
  }
  u8 *fb = vbe_get_framebuffer();
  u32 offset = y * g_device.pitch + x * (g_device.bpp / 8);
  u32 pixel = 0;
  switch (g_device.bpp) {
  case 8:
    pixel = *(u8 *)(fb + offset);
    break;
  case 15:
  case 16:
    pixel = *(u16 *)(fb + offset);
    break;
  case 24:
    pixel = *(u8 *)(fb + offset) | (*(u8 *)(fb + offset + 1) << 8) |
            (*(u8 *)(fb + offset + 2) << 16);
    break;
  case 32:
    pixel = *(u32 *)(fb + offset);
    break;
  default:
    return color;
  }
  return vbe_pixel_to_color(pixel);
}

kernel_status_t vbe_fill_rect(u16 x, u16 y, u16 width, u16 height,
                              vbe_color_t color) {
  if (!g_device.initialized || x + width > g_device.width ||
      y + height > g_device.height) {
    return KERNEL_ERROR;
  }

  u8 *fb = vbe_get_framebuffer();
  u32 pixel = vbe_color_to_pixel(color);
  u32 bpp_bytes = g_device.bpp / 8;
  u32 row_start = y * g_device.pitch + x * bpp_bytes;
  u32 pat = vbe_replicate_pixel(pixel, g_device.bpp);

  for (u16 py = 0; py < height; ++py) {
    u8 *dst = fb + row_start;
    u32 remaining = width;
    u32 dst_idx = (u32)dst % 4; /* Alignment offset (assume 32-bit words) */

    if (dst_idx) {
      u32 left_pixels = MIN(remaining, (4 - dst_idx) / bpp_bytes);
      for (u32 i = 0; i < left_pixels; ++i) {
        switch (g_device.bpp) {
        case 8:
          *dst = (u8)pixel;
          break;
        case 15:
        case 16:
          *(u16 *)dst = (u16)pixel;
          break;
        case 24:
          dst[0] = (u8)(pixel & 0xFF);
          dst[1] = (u8)((pixel >> 8) & 0xFF);
          dst[2] = (u8)((pixel >> 16) & 0xFF);
          break;
        case 32:
          *(u32 *)dst = pixel;
          break;
        }
        dst += bpp_bytes;
      }
      remaining -= left_pixels;
    }

    u32 aligned_words = (remaining * bpp_bytes) / 4;
    u32 *dst32 = (u32 *)dst;
    for (u32 i = 0; i < aligned_words; ++i) {
      VBE_WRITE32(dst32 + i, pat);
    }
    u32 filled_pixels = aligned_words * (4 / bpp_bytes);
    dst += filled_pixels * bpp_bytes;
    remaining -= filled_pixels;

    for (u32 i = 0; i < remaining; ++i) {
      switch (g_device.bpp) {
      case 8:
        *dst = (u8)pixel;
        break;
      case 15:
      case 16:
        *(u16 *)dst = (u16)pixel;
        break;
      case 24:
        dst[0] = (u8)(pixel & 0xFF);
        dst[1] = (u8)((pixel >> 8) & 0xFF);
        dst[2] = (u8)((pixel >> 16) & 0xFF);
        break;
      case 32:
        *(u32 *)dst = pixel;
        break;
      }
      dst += bpp_bytes;
    }

    row_start += g_device.pitch;
  }
  return KERNEL_OK;
}

kernel_status_t vbe_draw_rect(u16 x, u16 y, u16 width, u16 height,
                              vbe_color_t color) {
  kernel_status_t status;
  status = vbe_draw_horizontal_line(x, y, width, color);
  if (status != KERNEL_OK)
    return status;
  status = vbe_draw_horizontal_line(x, y + height - 1, width, color);
  if (status != KERNEL_OK)
    return status;
  status = vbe_draw_vertical_line(x, y, height, color);
  if (status != KERNEL_OK)
    return status;
  status = vbe_draw_vertical_line(x + width - 1, y, height, color);
  return status;
}

kernel_status_t vbe_clear_screen(vbe_color_t color) {
  return vbe_fill_rect(0, 0, g_device.width, g_device.height, color);
}

u16 vbe_get_width(void) {
  vbe_device_t *dev = vbe_get_device();
  return dev && dev->initialized ? dev->width : 0;
}

u16 vbe_get_height(void) {
  vbe_device_t *dev = vbe_get_device();
  return dev && dev->initialized ? dev->height : 0;
}

u8 vbe_get_bpp(void) {
  vbe_device_t *dev = vbe_get_device();
  return dev && dev->initialized ? dev->bpp : 0;
}

void vbe_draw_string(u16 x, u16 y, const char *str, vbe_color_t fg,
                     vbe_color_t bg) {
  font_render_string(str, x, y, fg, bg, font_get_default());
}

void vbe_draw_string_centered(u16 y, const char *str, vbe_color_t fg,
                              vbe_color_t bg) {
  const font_t *font = font_get_default();
  size_t len = strlen(str);
  u16 screen_width = vbe_get_width();
  u16 str_width = len * font->width;
  u16 x = (screen_width > str_width) ? (screen_width - str_width) / 2 : 0;
  if (y >= vbe_get_height()) {
    return;
  }
  vbe_draw_string(x, y, str, fg, bg);
}

kernel_status_t vbe_draw_line(u16 x1, u16 y1, u16 x2, u16 y2,
                              vbe_color_t color) {
  int deltax = (int)x2 - (int)x1;
  int dx = deltax >= 0 ? deltax : -deltax;
  int sx = x1 < x2 ? 1 : -1;
  int deltay = (int)y2 - (int)y1;
  int abs_deltay = deltay >= 0 ? deltay : -deltay;
  int dy = -abs_deltay;
  int sy = y1 < y2 ? 1 : -1;
  int err = dx + dy;
  int e2;

  while (true) {
    if (vbe_put_pixel(x1, y1, color) != KERNEL_OK) {
      return KERNEL_ERROR;
    }
    if (x1 == x2 && y1 == y2) {
      break;
    }
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x1 = (u16)((int)x1 + sx);
    }
    if (e2 <= dx) {
      err += dx;
      y1 = (u16)((int)y1 + sy);
    }
  }
  return KERNEL_OK;
}

kernel_status_t vbe_draw_horizontal_line(u16 x, u16 y, u16 width,
                                         vbe_color_t color) {
  for (u16 i = 0; i < width; ++i) {
    if (vbe_put_pixel(x + i, y, color) != KERNEL_OK) {
      return KERNEL_ERROR;
    }
  }
  return KERNEL_OK;
}

kernel_status_t vbe_draw_vertical_line(u16 x, u16 y, u16 height,
                                       vbe_color_t color) {
  for (u16 i = 0; i < height; ++i) {
    if (vbe_put_pixel(x, y + i, color) != KERNEL_OK) {
      return KERNEL_ERROR;
    }
  }
  return KERNEL_OK;
}

kernel_status_t vbe_show_info(void) {
  printf("VBE Version: %x\n", g_device.control_info.version);
  if (g_device.control_info.oem_string_ptr &&
      vmm_get_physical_addr(g_device.control_info.oem_string_ptr) != 0) {
    printf("OEM String: %s\n", (char *)g_device.control_info.oem_string_ptr);
  } else {
    printf("OEM String: (unavailable)\n");
  }
  printf("Total Memory: %u KB\n", (u32)g_device.control_info.total_memory * 64);
  printf("Current Mode: 0x%x\n", g_device.current_mode);
  printf("Resolution: %ux%u\n", g_device.width, g_device.height);
  printf("BPP: %u\n", g_device.bpp);
  printf("Pitch: %u\n", g_device.pitch);
  printf("Framebuffer: 0x%x\n", g_device.framebuffer_addr);
  return KERNEL_OK;
}

kernel_status_t vbe_list_modes(void) {
  u32 seg = g_device.control_info.video_modes_ptr >> 16;
  u32 off = g_device.control_info.video_modes_ptr & 0xFFFF;
  u32 phys_addr = seg * 16 + off;
  u16 *modes = (u16 *)phys_addr;
  u32 page_boundary = ALIGN_UP(phys_addr, PAGE_SIZE);

  printf("Supported VBE Modes:\n");
  while (*modes != 0xFFFF && (u32)modes < page_boundary) {
    printf("0x%x\n", *modes);
    ++modes;
  }
  if ((u32)modes >= page_boundary) {
    log(LOG_WARN, "VBE: Mode list exceeds mapped page boundary");
  }
  return KERNEL_OK;
}

kernel_status_t vbe_scroll() {
  if (!g_device.initialized)
    return KERNEL_ERROR;

  const font_t *font = font_get_default();
  u16 line_height = font ? font->height : 16;

  if (line_height >= g_device.height)
    return KERNEL_ERROR;

  vbe_blit(0, 0, 0, line_height, g_device.width, g_device.height - line_height);

  vbe_color_t bg_color = g_terminal.bg_color;

  vbe_fill_rect(0, g_device.height - line_height, g_device.width, line_height,
                bg_color);

  return KERNEL_OK;
}

kernel_status_t vbe_draw_circle(u16 cx, u16 cy, u16 radius, vbe_color_t color) {
  if (!g_device.initialized)
    return KERNEL_ERROR;
  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    vbe_put_pixel(cx + x, cy + y, color);
    vbe_put_pixel(cx + y, cy + x, color);
    vbe_put_pixel(cx - y, cy + x, color);
    vbe_put_pixel(cx - x, cy + y, color);
    vbe_put_pixel(cx - x, cy - y, color);
    vbe_put_pixel(cx - y, cy - x, color);
    vbe_put_pixel(cx + y, cy - x, color);
    vbe_put_pixel(cx + x, cy - y, color);

    y += 1;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x -= 1;
      err += 1 - 2 * x;
    }
  }
  return KERNEL_OK;
}

kernel_status_t vbe_blit(u16 dst_x, u16 dst_y, u16 src_x, u16 src_y, u16 width,
                         u16 height) {
  if (!g_device.initialized)
    return KERNEL_ERROR;
  if (dst_x + width > g_device.width || dst_y + height > g_device.height ||
      src_x + width > g_device.width || src_y + height > g_device.height)
    return KERNEL_INVALID_PARAM;

  u8 *fb = vbe_get_framebuffer();
  u32 bpp_bytes = g_device.bpp / 8;

  for (u16 py = 0; py < height; ++py) {
    u8 *src = fb + (src_y + py) * g_device.pitch + src_x * bpp_bytes;
    u8 *dst = fb + (dst_y + py) * g_device.pitch + dst_x * bpp_bytes;
    u32 remaining = width;
    u32 src_align = (u32)src % 4;
    u32 dst_align = (u32)dst % 4;

    if (src_align != dst_align) {
      for (u32 px = 0; px < width; ++px) {
        u32 pixel = 0;
        switch (g_device.bpp) {
        case 8:
          pixel = *src;
          *dst = (u8)pixel;
          break;
        case 15:
        case 16:
          pixel = *(u16 *)src;
          *(u16 *)dst = (u16)pixel;
          break;
        case 24:
          dst[0] = src[0];
          dst[1] = src[1];
          dst[2] = src[2];
          break;
        case 32:
          *(u32 *)dst = *(u32 *)src;
          break;
        default:
          return KERNEL_NOT_IMPLEMENTED;
        }
        src += bpp_bytes;
        dst += bpp_bytes;
      }
      continue;
    }

    u32 left_pixels =
        (dst_align != 0) ? MIN(remaining, (4 - dst_align) / bpp_bytes) : 0;
    for (u32 i = 0; i < left_pixels; ++i) {
      switch (g_device.bpp) {
      case 8:
        *dst = *src;
        break;
      case 15:
      case 16:
        *(u16 *)dst = *(u16 *)src;
        break;
      case 24:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        break;
      case 32:
        *(u32 *)dst = *(u32 *)src;
        break;
      }
      src += bpp_bytes;
      dst += bpp_bytes;
    }
    remaining -= left_pixels;

    u32 aligned_words = (remaining * bpp_bytes) / 4;
    u32 *src32 = (u32 *)src;
    u32 *dst32 = (u32 *)dst;
    for (u32 i = 0; i < aligned_words; ++i) {
      VBE_WRITE32(dst32 + i, *(src32 + i));
    }
    u32 filled_pixels = aligned_words * (4 / bpp_bytes);
    src += filled_pixels * bpp_bytes;
    dst += filled_pixels * bpp_bytes;
    remaining -= filled_pixels;

    for (u32 i = 0; i < remaining; ++i) {
      switch (g_device.bpp) {
      case 8:
        *dst = *src;
        break;
      case 15:
      case 16:
        *(u16 *)dst = *(u16 *)src;
        break;
      case 24:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        break;
      case 32:
        *(u32 *)dst = *(u32 *)src;
        break;
      }
      src += bpp_bytes;
      dst += bpp_bytes;
    }
  }
  return KERNEL_OK;
}