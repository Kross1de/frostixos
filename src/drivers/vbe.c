#include <arch/i386/multiboot.h>
#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <misc/logger.h>
#include <mm/vmm.h>
#include <printf.h>

extern u32 _multiboot_info_ptr;

static vbe_device_t g_device = {0};

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
  if (!g_device.initialized) {
    return KERNEL_ERROR;
  }
  for (u16 py = y; py < y + height; ++py) {
    for (u16 px = x; px < x + width; ++px) {
      if (vbe_put_pixel(px, py, color) != KERNEL_OK) {
        return KERNEL_ERROR;
      }
    }
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
  if (!g_device.initialized) {
    return KERNEL_ERROR;
  }
  if (dst_x + width > g_device.width || dst_y + height > g_device.height ||
      src_x + width > g_device.width || src_y + height > g_device.height) {
    return KERNEL_INVALID_PARAM;
  }
  u8 *fb = vbe_get_framebuffer();
  u32 bpp_bytes = g_device.bpp / 8;
  for (u16 i = 0; i < height; ++i) {
    void *src_ptr = fb + (src_y + i) * g_device.pitch + src_x * bpp_bytes;
    void *dst_ptr = fb + (dst_y + i) * g_device.pitch + dst_x * bpp_bytes;
    memmove(dst_ptr, src_ptr, width * bpp_bytes);
  }
  return KERNEL_OK;
}