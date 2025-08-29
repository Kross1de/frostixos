#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <arch/i386/multiboot.h>

static vbe_device_t g_vbe_device = {0};
static u8* g_vbe_framebuffer = NULL;

static bool vbe_find_multiboot_info(multiboot_info_t** mbi_out, multiboot_vbe_info_t** vbe_ctrl_out, multiboot_vbe_mode_info_t** vbe_mode_out) {
    extern u32 _multiboot_info_ptr;
    multiboot_info_t* mbi = (multiboot_info_t*)_multiboot_info_ptr;
    if (!mbi || !(mbi->flags & (1 << 11)))
        return false;
    if (mbi_out) *mbi_out = mbi;
    if (vbe_ctrl_out) *vbe_ctrl_out = (multiboot_vbe_info_t*)(uintptr_t)mbi->vbe_control_info;
    if (vbe_mode_out) *vbe_mode_out = (multiboot_vbe_mode_info_t*)(uintptr_t)mbi->vbe_mode_info;
    return true;
}

static void vbe_copy_control_info(vbe_control_info_t* dst, const multiboot_vbe_info_t* src) {
    memcpy(dst, src, sizeof(vbe_control_info_t));
}
static void vbe_copy_mode_info(vbe_mode_info_t* dst, const multiboot_vbe_mode_info_t* src) {
    memcpy(dst, src, sizeof(vbe_mode_info_t));
}

kernel_status_t vbe_init(void) {
    multiboot_info_t* mbi = NULL;
    multiboot_vbe_info_t* vbe_ctrl = NULL;
    multiboot_vbe_mode_info_t* vbe_mode = NULL;
    if (!vbe_find_multiboot_info(&mbi, &vbe_ctrl, &vbe_mode))
        return KERNEL_ERROR;

    memset(&g_vbe_device, 0, sizeof(g_vbe_device));
    g_vbe_device.initialized = true;
    g_vbe_device.current_mode = mbi->vbe_mode;
    vbe_copy_control_info(&g_vbe_device.control_info, vbe_ctrl);
    vbe_copy_mode_info(&g_vbe_device.mode_info, vbe_mode);

    g_vbe_device.width = vbe_mode->x_resolution;
    g_vbe_device.height = vbe_mode->y_resolution;
    g_vbe_device.bpp = vbe_mode->bits_per_pixel;
    g_vbe_device.pitch = vbe_mode->lin_bytes_per_scan_line ? vbe_mode->lin_bytes_per_scan_line : vbe_mode->bytes_per_scanline;
    g_vbe_device.framebuffer_addr = vbe_mode->phys_base_ptr;
    g_vbe_device.memory_model = vbe_mode->memory_model;
    g_vbe_device.linear_supported = (vbe_mode->mode_attributes & VBE_MODE_ATTR_LINEAR) ? true : false;

    if (g_vbe_device.linear_supported) {
        g_vbe_device.red_mask_size = vbe_mode->lin_red_mask_size;
        g_vbe_device.red_field_position = vbe_mode->lin_red_field_position;
        g_vbe_device.green_mask_size = vbe_mode->lin_green_mask_size;
        g_vbe_device.green_field_position = vbe_mode->lin_green_field_position;
        g_vbe_device.blue_mask_size = vbe_mode->lin_blue_mask_size;
        g_vbe_device.blue_field_position = vbe_mode->lin_blue_field_position;
    } else {
        g_vbe_device.red_mask_size = vbe_mode->red_mask_size;
        g_vbe_device.red_field_position = vbe_mode->red_field_position;
        g_vbe_device.green_mask_size = vbe_mode->green_mask_size;
        g_vbe_device.green_field_position = vbe_mode->green_field_position;
        g_vbe_device.blue_mask_size = vbe_mode->blue_mask_size;
        g_vbe_device.blue_field_position = vbe_mode->blue_field_position;
    }

    g_vbe_device.framebuffer_size = g_vbe_device.pitch * g_vbe_device.height;

    g_vbe_framebuffer = (u8*)(uintptr_t)g_vbe_device.framebuffer_addr;
    return KERNEL_OK;
}

bool vbe_is_available(void) {
    return g_vbe_device.initialized && g_vbe_device.framebuffer_addr != 0;
}

vbe_device_t* vbe_get_device(void) {
    return vbe_is_available() ? &g_vbe_device : NULL;
}

u8* vbe_get_framebuffer(void) {
    return vbe_is_available() ? g_vbe_framebuffer : NULL;
}

u32 vbe_color_to_pixel(vbe_color_t color) {
    vbe_device_t* dev = vbe_get_device();
    if (!dev) return 0;

    if (dev->bpp == 32) {
        return ((color.red & 0xFF) << dev->red_field_position) |
               ((color.green & 0xFF) << dev->green_field_position) |
               ((color.blue & 0xFF) << dev->blue_field_position) |
               ((color.alpha & 0xFF) << 24);
    } else if (dev->bpp == 24) {
        return ((color.red & 0xFF) << dev->red_field_position) |
               ((color.green & 0xFF) << dev->green_field_position) |
               ((color.blue & 0xFF) << dev->blue_field_position);
    } else if (dev->bpp == 16) {
        return (((color.red >> 3) & 0x1F) << 11) |
               (((color.green >> 2) & 0x3F) << 5) |
               (((color.blue >> 3) & 0x1F) << 0);
    } else if (dev->bpp == 8) {
        return (color.red + color.green + color.blue) / 3;
    }
    return 0;
}

vbe_color_t vbe_pixel_to_color(u32 pixel) {
    vbe_color_t color = {0,0,0,255};
    vbe_device_t* dev = vbe_get_device();
    if (!dev) return color;

    if (dev->bpp == 32) {
        color.red   = (pixel >> dev->red_field_position) & 0xFF;
        color.green = (pixel >> dev->green_field_position) & 0xFF;
        color.blue  = (pixel >> dev->blue_field_position) & 0xFF;
        color.alpha = (pixel >> 24) & 0xFF;
    } else if (dev->bpp == 24) {
        color.red   = (pixel >> dev->red_field_position) & 0xFF;
        color.green = (pixel >> dev->green_field_position) & 0xFF;
        color.blue  = (pixel >> dev->blue_field_position) & 0xFF;
        color.alpha = 0xFF;
    } else if (dev->bpp == 16) {
        color.red   = ((pixel >> 11) & 0x1F) << 3;
        color.green = ((pixel >> 5) & 0x3F) << 2;
        color.blue  = (pixel & 0x1F) << 3;
        color.alpha = 0xFF;
    } else if (dev->bpp == 8) {
        color.red = color.green = color.blue = pixel & 0xFF;
        color.alpha = 0xFF;
    }
    return color;
}

kernel_status_t vbe_set_mode(u16 mode) {
    UNUSED(mode);
    return KERNEL_NOT_IMPLEMENTED;
}

kernel_status_t vbe_get_mode_info(u16 mode, vbe_mode_info_t* mode_info) {
    UNUSED(mode); UNUSED(mode_info);
    return KERNEL_NOT_IMPLEMENTED;
}

kernel_status_t vbe_map_framebuffer(void) {
    return vbe_is_available() ? KERNEL_OK : KERNEL_ERROR;
}

kernel_status_t vbe_put_pixel(u16 x, u16 y, vbe_color_t color) {
    vbe_device_t* dev = vbe_get_device();
    if (!dev || x >= dev->width || y >= dev->height)
        return KERNEL_INVALID_PARAM;
    if (!g_vbe_framebuffer)
        return KERNEL_ERROR;

    u32 pixel = vbe_color_to_pixel(color);
    u32 bytes_per_pixel = dev->bpp / 8;
    u32 offset = y * dev->pitch + x * bytes_per_pixel;
    u8* ptr = g_vbe_framebuffer + offset;

    switch (dev->bpp) {
        case 32:
            *(u32*)ptr = pixel;
            break;
        case 24:
            ptr[0] = (pixel >> 0) & 0xFF;
            ptr[1] = (pixel >> 8) & 0xFF;
            ptr[2] = (pixel >> 16) & 0xFF;
            break;
        case 16:
            *(u16*)ptr = (u16)pixel;
            break;
        case 8:
            *ptr = (u8)pixel;
            break;
        default:
            return KERNEL_NOT_IMPLEMENTED;
    }
    return KERNEL_OK;
}

vbe_color_t vbe_get_pixel(u16 x, u16 y) {
    vbe_color_t result = {0, 0, 0, 255};
    vbe_device_t* dev = vbe_get_device();
    if (!dev || x >= dev->width || y >= dev->height || !g_vbe_framebuffer)
        return result;

    u32 bytes_per_pixel = dev->bpp / 8;
    u32 offset = y * dev->pitch + x * bytes_per_pixel;
    u8* ptr = g_vbe_framebuffer + offset;
    u32 pixel = 0;

    switch (dev->bpp) {
        case 32:
            pixel = *(u32*)ptr;
            break;
        case 24:
            pixel = (ptr[2] << 16) | (ptr[1] << 8) | (ptr[0]);
            break;
        case 16:
            pixel = *(u16*)ptr;
            break;
        case 8:
            pixel = *ptr;
            break;
    }
    return vbe_pixel_to_color(pixel);
}

kernel_status_t vbe_fill_rect(u16 x, u16 y, u16 width, u16 height, vbe_color_t color) {
    vbe_device_t* dev = vbe_get_device();
    if (!dev) return KERNEL_ERROR;
    for (u16 dy = 0; dy < height; ++dy)
        for (u16 dx = 0; dx < width; ++dx)
            vbe_put_pixel(x + dx, y + dy, color);
    return KERNEL_OK;
}

kernel_status_t vbe_draw_rect(u16 x, u16 y, u16 width, u16 height, vbe_color_t color) {
    if (width < 2 || height < 2)
        return KERNEL_INVALID_PARAM;
    vbe_draw_horizontal_line(x, y, width, color);
    vbe_draw_horizontal_line(x, y + height - 1, width, color);
    vbe_draw_vertical_line(x, y, height, color);
    vbe_draw_vertical_line(x + width - 1, y, height, color);
    return KERNEL_OK;
}

kernel_status_t vbe_clear_screen(vbe_color_t color) {
    vbe_device_t* dev = vbe_get_device();
    if (!dev) return KERNEL_ERROR;
    return vbe_fill_rect(0, 0, dev->width, dev->height, color);
}

kernel_status_t vbe_draw_horizontal_line(u16 x, u16 y, u16 width, vbe_color_t color) {
    vbe_device_t* dev = vbe_get_device();
    if (!dev || y >= dev->height)
        return KERNEL_INVALID_PARAM;
    for (u16 dx = 0; dx < width; ++dx)
        vbe_put_pixel(x + dx, y, color);
    return KERNEL_OK;
}

kernel_status_t vbe_draw_vertical_line(u16 x, u16 y, u16 height, vbe_color_t color) {
    vbe_device_t* dev = vbe_get_device();
    if (!dev || x >= dev->width)
        return KERNEL_INVALID_PARAM;
    for (u16 dy = 0; dy < height; ++dy)
        vbe_put_pixel(x, y + dy, color);
    return KERNEL_OK;
}

kernel_status_t vbe_draw_line(u16 x1, u16 y1, u16 x2, u16 y2, vbe_color_t color) {
    int dx = (int)x2 - (int)x1;
    int dy = (int)y2 - (int)y1;
    int dx1 = dx >= 0 ? dx : -dx;
    int dy1 = dy >= 0 ? dy : -dy;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    int err = (dx1 > dy1 ? dx1 : -dy1) / 2;
    int e2;

    int x = (int)x1, y = (int)y1;
    for (;;) {
        vbe_put_pixel(x, y, color);
        if (x == (int)x2 && y == (int)y2) break;
        e2 = err;
        if (e2 > -dx1) { err -= dy1; x += sx; }
        if (e2 <  dy1) { err += dx1; y += sy; }
    }
    return KERNEL_OK;
}

kernel_status_t vbe_show_info(void) {
    vbe_device_t* dev = vbe_get_device();
    if (!dev) return KERNEL_ERROR;
    return KERNEL_NOT_IMPLEMENTED;
}

kernel_status_t vbe_list_modes(void) {
    return KERNEL_NOT_IMPLEMENTED;
}
