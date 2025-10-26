/*
 * VBE driver.
 */
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

static vbe_device_t g_device = { 0 };

#define VBE_WRITE32(addr, val) (*(volatile u32 *)(addr) = (val))

/* Replicate pixel to 32-bit word for fast fill */
static u32 vbe_replicate_pixel(u32 pixel, u8 bpp)
{
	if (bpp == 32)
		return pixel;
	if (bpp == 16 || bpp == 15)
		return (pixel << 16) | pixel;
	if (bpp == 8)
		return (pixel << 24) | (pixel << 16) | (pixel << 8) | pixel;
	if (bpp == 24) {
		/* approximate replication for u32 stores */
		u32 rgb = pixel & 0xFFFFFF;
		return (rgb << 8) | (rgb >> 16);
	}
	return pixel;
}

kernel_status_t vbe_init(void)
{
	multiboot_info_t *mbi = (multiboot_info_t *)_multiboot_info_ptr;
	if (!mbi || !(mbi->flags & (1 << 11)))
		return KERNEL_ERROR;

	memcpy(&g_device.control_info, (void *)mbi->vbe_control_info,
	       sizeof(vbe_control_info_t));
	memcpy(&g_device.mode_info, (void *)mbi->vbe_mode_info,
	       sizeof(vbe_mode_info_t));

	g_device.current_mode     = mbi->vbe_mode;
	g_device.framebuffer_addr = g_device.mode_info.phys_base_ptr;
	g_device.width            = g_device.mode_info.x_resolution;
	g_device.height           = g_device.mode_info.y_resolution;
	g_device.bpp              = g_device.mode_info.bits_per_pixel;
	g_device.pitch            = g_device.mode_info.bytes_per_scanline ?
					   g_device.mode_info.bytes_per_scanline :
					   g_device.mode_info.lin_bytes_per_scan_line;
	g_device.memory_model     = g_device.mode_info.memory_model;

	/* Choose masks from linear fields if present */
	g_device.red_mask_size   = g_device.mode_info.red_mask_size;
	g_device.red_field_position = g_device.mode_info.red_field_position;
	g_device.green_mask_size = g_device.mode_info.green_mask_size;
	g_device.green_field_position = g_device.mode_info.green_field_position;
	g_device.blue_mask_size  = g_device.mode_info.blue_mask_size;
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
	g_device.linear_supported = (g_device.mode_info.mode_attributes & VBE_MODE_ATTR_LINEAR) != 0;
	g_device.initialized = true;

	return KERNEL_OK;
}

vbe_device_t *vbe_get_device(void)
{
	return &g_device;
}

bool vbe_is_available(void)
{
	return g_device.initialized;
}

kernel_status_t vbe_set_mode(u16 mode)
{
	UNUSED(mode);
	return KERNEL_NOT_IMPLEMENTED;
}

kernel_status_t vbe_get_mode_info(u16 mode, vbe_mode_info_t *mode_info)
{
	if (!mode_info || mode != g_device.current_mode)
		return KERNEL_INVALID_PARAM;

	memcpy(mode_info, &g_device.mode_info, sizeof(vbe_mode_info_t));
	return KERNEL_OK;
}

u8 *vbe_get_framebuffer(void)
{
	return (u8 *)(uintptr_t)g_device.framebuffer_addr;
}

u32 vbe_color_to_pixel(vbe_color_t color)
{
	u32 r = (g_device.red_mask_size) ? ((color.red * ((1u << g_device.red_mask_size) - 1)) / 255u) : 0;
	u32 g = (g_device.green_mask_size) ? ((color.green * ((1u << g_device.green_mask_size) - 1)) / 255u) : 0;
	u32 b = (g_device.blue_mask_size) ? ((color.blue * ((1u << g_device.blue_mask_size) - 1)) / 255u) : 0;
	u32 a = 0;

	return (r << g_device.red_field_position)
	       | (g << g_device.green_field_position)
	       | (b << g_device.blue_field_position)
	       | (a << g_device.mode_info.rsvd_field_position);
}

vbe_color_t vbe_pixel_to_color(u32 pixel)
{
	vbe_color_t color = { 0, 0, 0, 0 };
	u32 r_mask = (g_device.red_mask_size) ? ((1u << g_device.red_mask_size) - 1) : 1;
	u32 g_mask = (g_device.green_mask_size) ? ((1u << g_device.green_mask_size) - 1) : 1;
	u32 b_mask = (g_device.blue_mask_size) ? ((1u << g_device.blue_mask_size) - 1) : 1;

	color.red   = (u8)(((pixel >> g_device.red_field_position) & r_mask) * 255 / (r_mask ? r_mask : 1));
	color.green = (u8)(((pixel >> g_device.green_field_position) & g_mask) * 255 / (g_mask ? g_mask : 1));
	color.blue  = (u8)(((pixel >> g_device.blue_field_position) & b_mask) * 255 / (b_mask ? b_mask : 1));
	color.alpha = 255;
	return color;
}

kernel_status_t vbe_put_pixel(u16 x, u16 y, vbe_color_t color)
{
	if (!g_device.initialized || x >= g_device.width || y >= g_device.height)
		return KERNEL_INVALID_PARAM;

	u8 *fb = vbe_get_framebuffer();
	u32 offset = (u32)y * g_device.pitch + (u32)x * (g_device.bpp / 8);
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
		fb[offset + 0] = (u8)(pixel & 0xFF);
		fb[offset + 1] = (u8)((pixel >> 8) & 0xFF);
		fb[offset + 2] = (u8)((pixel >> 16) & 0xFF);
		break;
	case 32:
		*(u32 *)(fb + offset) = pixel;
		break;
	default:
		return KERNEL_INVALID_PARAM;
	}

	return KERNEL_OK;
}

vbe_color_t vbe_get_pixel(u16 x, u16 y)
{
	vbe_color_t color = { 0, 0, 0, 0 };

	if (!g_device.initialized || x >= g_device.width || y >= g_device.height)
		return color;

	u8 *fb = vbe_get_framebuffer();
	u32 offset = (u32)y * g_device.pitch + (u32)x * (g_device.bpp / 8);
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
		pixel = fb[offset] | (fb[offset + 1] << 8) | (fb[offset + 2] << 16);
		break;
	case 32:
		pixel = *(u32 *)(fb + offset);
		break;
	default:
		return color;
	}

	return vbe_pixel_to_color(pixel);
}

kernel_status_t vbe_fill_rect(u16 x, u16 y, u16 width, u16 height, vbe_color_t color)
{
	if (!g_device.initialized || x + width > g_device.width || y + height > g_device.height)
		return KERNEL_INVALID_PARAM;

	u8 *fb = vbe_get_framebuffer();
	u32 pixel = vbe_color_to_pixel(color);
	u32 bpp_bytes = g_device.bpp / 8;
	u32 pat = vbe_replicate_pixel(pixel, g_device.bpp);

	for (u16 row = 0; row < height; row++) {
		u8 *dst = fb + (y + row) * g_device.pitch + x * bpp_bytes;
		u32 remaining = width;

		/* Align to 32-bit boundary and write replicated pattern */
		while (remaining >= 4 / bpp_bytes) {
			VBE_WRITE32(dst, pat);
			dst += 4;
			remaining -= 4 / bpp_bytes;
		}

		/* Tail */
		for (u32 i = 0; i < remaining; i++) {
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
	}

	return KERNEL_OK;
}

kernel_status_t vbe_draw_rect(u16 x, u16 y, u16 width, u16 height, vbe_color_t color)
{
	kernel_status_t st;

	st = vbe_draw_horizontal_line(x, y, width, color);
	if (st != KERNEL_OK) return st;
	st = vbe_draw_horizontal_line(x, y + height - 1, width, color);
	if (st != KERNEL_OK) return st;
	st = vbe_draw_vertical_line(x, y, height, color);
	if (st != KERNEL_OK) return st;
	st = vbe_draw_vertical_line(x + width - 1, y, height, color);
	return st;
}

kernel_status_t vbe_clear_screen(vbe_color_t color)
{
	return vbe_fill_rect(0, 0, g_device.width, g_device.height, color);
}

u16 vbe_get_width(void)
{
	return g_device.initialized ? g_device.width : 0;
}

u16 vbe_get_height(void)
{
	return g_device.initialized ? g_device.height : 0;
}

u8 vbe_get_bpp(void)
{
	return g_device.initialized ? g_device.bpp : 0;
}

void vbe_draw_string(u16 x, u16 y, const char *str, vbe_color_t fg, vbe_color_t bg)
{
	font_render_string(str, x, y, fg, bg, font_get_default());
}

void vbe_draw_string_centered(u16 y, const char *str, vbe_color_t fg, vbe_color_t bg)
{
	const font_t *font = font_get_default();
	size_t len = strlen(str);
	u16 str_w = len * font->width;
	u16 screen_w = vbe_get_width();
	u16 x = (screen_w > str_w) ? (screen_w - str_w) / 2 : 0;
	if (y >= vbe_get_height())
		return;
	vbe_draw_string(x, y, str, fg, bg);
}

kernel_status_t vbe_draw_line(u16 x1, u16 y1, u16 x2, u16 y2, vbe_color_t color)
{
	int dx = (int)x2 - (int)x1;
	int dy = (int)y2 - (int)y1;
	int abs_dx = dx >= 0 ? dx : -dx;
	int abs_dy = dy >= 0 ? dy : -dy;
	int sx = x1 < x2 ? 1 : -1;
	int sy = y1 < y2 ? 1 : -1;
	int err = (abs_dx > abs_dy ? abs_dx : -abs_dy) / 2;
	int e2;

	while (1) {
		if (vbe_put_pixel(x1, y1, color) != KERNEL_OK)
			return KERNEL_ERROR;
		if (x1 == x2 && y1 == y2)
			break;
		e2 = err;
		if (e2 > -abs_dx) { err -= abs_dy; x1 = (u16)((int)x1 + sx); }
		if (e2 < abs_dy)  { err += abs_dx; y1 = (u16)((int)y1 + sy); }
	}

	return KERNEL_OK;
}

kernel_status_t vbe_draw_horizontal_line(u16 x, u16 y, u16 width, vbe_color_t color)
{
	for (u16 i = 0; i < width; ++i) {
		if (vbe_put_pixel(x + i, y, color) != KERNEL_OK)
			return KERNEL_ERROR;
	}
	return KERNEL_OK;
}

kernel_status_t vbe_draw_vertical_line(u16 x, u16 y, u16 height, vbe_color_t color)
{
	for (u16 i = 0; i < height; ++i) {
		if (vbe_put_pixel(x, y + i, color) != KERNEL_OK)
			return KERNEL_ERROR;
	}
	return KERNEL_OK;
}

/* Debug helpers */
void vbe_show_info(void)
{
	printf("VBE Version: %x\n", g_device.control_info.version);
	if (g_device.control_info.oem_string_ptr)
		printf("OEM String: %s\n", (char *)g_device.control_info.oem_string_ptr);
	else
		printf("OEM String: (unavailable)\n");
	printf("Total Memory: %u KB\n", (u32)g_device.control_info.total_memory * 64);
	printf("Current Mode: 0x%x\n", g_device.current_mode);
	printf("Resolution: %ux%u\n", g_device.width, g_device.height);
	printf("BPP: %u\n", g_device.bpp);
	printf("Pitch: %u\n", g_device.pitch);
	printf("Framebuffer: 0x%x\n", g_device.framebuffer_addr);
}

void vbe_list_modes(void)
{
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
	if ((u32)modes >= page_boundary)
		log(LOG_WARN, "VBE: mode list crosses page boundary");
}

kernel_status_t vbe_scroll(void)
{
	if (!g_device.initialized)
		return KERNEL_ERROR;

	const font_t *font = font_get_default();
	u16 line_h = font->height;
	if (line_h >= g_device.height)
		return KERNEL_ERROR;

	vbe_blit(0, 0, 0, line_h, g_device.width, g_device.height - line_h);
	vbe_fill_rect(0, g_device.height - line_h, g_device.width, line_h, g_terminal.bg_color);

	return KERNEL_OK;
}

kernel_status_t vbe_draw_circle(u16 cx, u16 cy, u16 radius, vbe_color_t color)
{
	if (!g_device.initialized)
		return KERNEL_ERROR;

	int x = radius, y = 0, err = 0;
	while (x >= y) {
		vbe_put_pixel(cx + x, cy + y, color);
		vbe_put_pixel(cx + y, cy + x, color);
		vbe_put_pixel(cx - y, cy + x, color);
		vbe_put_pixel(cx - x, cy + y, color);
		vbe_put_pixel(cx - x, cy - y, color);
		vbe_put_pixel(cx - y, cy - x, color);
		vbe_put_pixel(cx + y, cy - x, color);
		vbe_put_pixel(cx + x, cy - y, color);

		y++;
		err += 1 + 2*y;
		if (2*(err - x) + 1 > 0) {
			x--;
			err += 1 - 2*x;
		}
	}
	return KERNEL_OK;
}

kernel_status_t vbe_blit(u16 dst_x, u16 dst_y, u16 src_x, u16 src_y, u16 width, u16 height)
{
	if (!g_device.initialized)
		return KERNEL_ERROR;

	if (dst_x + width > g_device.width || dst_y + height > g_device.height ||
	    src_x + width > g_device.width || src_y + height > g_device.height)
		return KERNEL_INVALID_PARAM;

	u8 *fb = vbe_get_framebuffer();
	u32 bpp_bytes = g_device.bpp / 8;

	for (u16 row = 0; row < height; ++row) {
		u8 *src = fb + (src_y + row) * g_device.pitch + src_x * bpp_bytes;
		u8 *dst = fb + (dst_y + row) * g_device.pitch + dst_x * bpp_bytes;
		for (u16 col = 0; col < width; ++col) {
			/* Copy pixel by pixel (handles arbitrary alignment) */
			switch (g_device.bpp) {
			case 8:
				dst[col] = src[col];
				break;
			case 16:
			case 15:
				((u16 *)dst)[col] = ((u16 *)src)[col];
				break;
			case 24:
				dst[col*3 + 0] = src[col*3 + 0];
				dst[col*3 + 1] = src[col*3 + 1];
				dst[col*3 + 2] = src[col*3 + 2];
				break;
			case 32:
				((u32 *)dst)[col] = ((u32 *)src)[col];
				break;
			default:
				return KERNEL_NOT_IMPLEMENTED;
			}
		}
	}
	return KERNEL_OK;
}