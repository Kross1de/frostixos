#ifndef DRIVERS_VBE_H
#define DRIVERS_VBE_H

#include <arch/i386/multiboot.h>
#include <kernel/kernel.h>

/*
 * VBE (VESA BIOS Extensions) driver and helpers for graphics/framebuffer.
 * The driver exposes mode information, framebuffer access and basic drawing
 * primitives used by higher-level text/font and UI code.
 */

/* VBE version codes */
#define VBE_VERSION_1_0 0x0100
#define VBE_VERSION_1_1 0x0101
#define VBE_VERSION_1_2 0x0102
#define VBE_VERSION_2_0 0x0200
#define VBE_VERSION_3_0 0x0300

/* Mode attribute flags */
#define VBE_MODE_ATTR_SUPPORTED 0x0001	/* Mode supported in hardware */
#define VBE_MODE_ATTR_TTY 0x0004	/* TTY output functions supported */
#define VBE_MODE_ATTR_COLOR 0x0008	/* Color mode */
#define VBE_MODE_ATTR_GRAPHICS 0x0010	/* Graphics mode */
#define VBE_MODE_ATTR_VGA_COMPATIBLE 0x0020 /* Not VGA compatible */
#define VBE_MODE_ATTR_VGA_WINDOWED 0x0040   /* VGA windowed mode not available */
#define VBE_MODE_ATTR_LINEAR 0x0080	    /* Linear framebuffer mode available */

#define VBE_MEMORY_MODEL_TEXT 0x00
#define VBE_MEMORY_MODEL_CGA 0x01
#define VBE_MEMORY_MODEL_HERCULES 0x02
#define VBE_MEMORY_MODEL_PLANAR 0x03
#define VBE_MEMORY_MODEL_PACKED_PIXEL 0x04
#define VBE_MEMORY_MODEL_NON_CHAIN4 0x05
#define VBE_MEMORY_MODEL_DIRECT_COLOR 0x06
#define VBE_MEMORY_MODEL_YUV 0x07

/* Common mode constants */
#define VBE_MODE_640x480x8 0x101
#define VBE_MODE_800x600x8 0x103
#define VBE_MODE_1024x768x8 0x105
#define VBE_MODE_640x480x15 0x110
#define VBE_MODE_640x480x16 0x111
#define VBE_MODE_640x480x24 0x112
#define VBE_MODE_800x600x15 0x113
#define VBE_MODE_800x600x16 0x114
#define VBE_MODE_800x600x24 0x115
#define VBE_MODE_1024x768x15 0x116
#define VBE_MODE_1024x768x16 0x117
#define VBE_MODE_1024x768x24 0x118

/* VBE control information structure (as returned by BIOS/bootloader) */
typedef struct {
	char signature[4];	/* "VESA" */
	u16 version;		/* VBE version */
	u32 oem_string_ptr;	/* pointer to OEM string */
	u32 capabilities;	/* controller capabilities */
	u32 video_modes_ptr;	/* pointer to supported modes list */
	u16 total_memory;	/* total memory in 64KB blocks */

	/* VBE 2.0+ fields */
	u16 oem_software_rev;
	u32 oem_vendor_name_ptr;
	u32 oem_product_name_ptr;
	u32 oem_product_rev_ptr;
	u8 reserved[222];
	u8 oem_data[256];
} __attribute__((packed)) vbe_control_info_t;

/* VBE Mode information structure */
typedef struct {
	u16 mode_attributes;
	u8 win_a_attributes;
	u8 win_b_attributes;
	u16 win_granularity;
	u16 win_size;
	u16 win_a_segment;
	u16 win_b_segment;
	u32 win_func_ptr;
	u16 bytes_per_scanline;

	u16 x_resolution;
	u16 y_resolution;
	u8 x_char_size;
	u8 y_char_size;
	u8 number_of_planes;
	u8 bits_per_pixel;
	u8 number_of_banks;
	u8 memory_model;
	u8 bank_size;
	u8 number_of_image_pages;
	u8 reserved_1;

	u8 red_mask_size;
	u8 red_field_position;
	u8 green_mask_size;
	u8 green_field_position;
	u8 blue_mask_size;
	u8 blue_field_position;
	u8 rsvd_mask_size;
	u8 rsvd_field_position;
	u8 direct_color_mode_info;

	u32 phys_base_ptr;
	u32 reserved_2;
	u16 reserved_3;

	u16 lin_bytes_per_scan_line;
	u8 banked_number_of_image_pages;
	u8 lin_number_of_image_pages;
	u8 lin_red_mask_size;
	u8 lin_red_field_position;
	u8 lin_green_mask_size;
	u8 lin_green_field_position;
	u8 lin_blue_mask_size;
	u8 lin_blue_field_position;
	u8 lin_rsvd_mask_size;
	u8 lin_rsvd_field_position;
	u32 max_pixel_clock;
	u8 reserved_4[189];
} __attribute__((packed)) vbe_mode_info_t;

/* Driver-visible device structure */
typedef struct {
	bool initialized;	      /* vbe subsystem initialized */
	bool linear_supported;	  /* linear framebuffer available */
	u16 current_mode;	      /* current VBE mode */
	u32 framebuffer_addr;     /* physical address of framebuffer */
	u32 framebuffer_size;     /* size in bytes */
	u16 width;		      /* screen width in pixels */
	u16 height;		      /* screen height in pixels */
	u8 bpp;		      /* bits per pixel */
	u16 pitch;		      /* bytes per scanline */
	u8 memory_model;	      /* memory model */

	u8 red_mask_size;
	u8 red_field_position;
	u8 green_mask_size;
	u8 green_field_position;
	u8 blue_mask_size;
	u8 blue_field_position;
	u8 rsvd_mask_size;
	u8 rsvd_field_position;

	vbe_control_info_t control_info;
	vbe_mode_info_t mode_info;
} vbe_device_t;

/* Simple RGBA color struct used by drawing routines */
typedef struct {
	u8 red;
	u8 green;
	u8 blue;
	u8 alpha;
} vbe_color_t;

/* Some common color constants */
#define VBE_COLOR_BLACK ((vbe_color_t){0, 0, 0, 255})
#define VBE_COLOR_WHITE ((vbe_color_t){255, 255, 255, 255})
#define VBE_COLOR_RED ((vbe_color_t){255, 0, 0, 255})
#define VBE_COLOR_GREEN ((vbe_color_t){0, 255, 0, 255})
#define VBE_COLOR_BLUE ((vbe_color_t){0, 0, 255, 255})
#define VBE_COLOR_CYAN ((vbe_color_t){0, 255, 255, 255})
#define VBE_COLOR_MAGENTA ((vbe_color_t){255, 0, 255, 255})
#define VBE_COLOR_YELLOW ((vbe_color_t){255, 255, 0, 255})
#define VBE_COLOR_GRAY ((vbe_color_t){128, 128, 128, 255})
#define VBE_COLOR_DARK_GRAY ((vbe_color_t){64, 64, 64, 255})
#define VBE_COLOR_ORANGE ((vbe_color_t){255, 128, 0, 255})
#define VBE_COLOR_BROWN ((vbe_color_t){128, 64, 0, 255})
#define VBE_COLOR_DARK_BLUE ((vbe_color_t){0, 0, 128, 255})
#define VBE_COLOR_LIGHT_GRAY ((vbe_color_t){192, 192, 192, 255})

/* VBE driver API */
kernel_status_t vbe_init(void);
kernel_status_t vbe_set_mode(u16 mode);
kernel_status_t vbe_get_mode_info(u16 mode, vbe_mode_info_t *mode_info);
vbe_device_t *vbe_get_device(void);
bool vbe_is_available(void);

kernel_status_t vbe_map_framebuffer(void);
u8 *vbe_get_framebuffer(void);
u32 vbe_color_to_pixel(vbe_color_t color);
vbe_color_t vbe_pixel_to_color(u32 pixel);

kernel_status_t vbe_put_pixel(u16 x, u16 y, vbe_color_t color);
vbe_color_t vbe_get_pixel(u16 x, u16 y);
kernel_status_t vbe_fill_rect(u16 x, u16 y, u16 width, u16 height,
			      vbe_color_t color);
kernel_status_t vbe_draw_rect(u16 x, u16 y, u16 width, u16 height,
			      vbe_color_t color);
kernel_status_t vbe_clear_screen(vbe_color_t color);

kernel_status_t vbe_draw_line(u16 x1, u16 y1, u16 x2, u16 y2,
			      vbe_color_t color);
kernel_status_t vbe_draw_horizontal_line(u16 x, u16 y, u16 width,
					 vbe_color_t color);
kernel_status_t vbe_draw_vertical_line(u16 x, u16 y, u16 height,
				       vbe_color_t color);

kernel_status_t vbe_draw_circle(u16 x, u16 y, u16 radius, vbe_color_t color);

u16 vbe_get_width(void);
u16 vbe_get_height(void);
u8 vbe_get_bpp(void);
kernel_status_t vbe_scroll(void);
kernel_status_t vbe_draw_filled_rect(u16 x, u16 y, u16 width, u16 height,
				     vbe_color_t fill, vbe_color_t border);
void vbe_draw_string(u16 x, u16 y, const char *str, vbe_color_t fg,
		     vbe_color_t bg);
void vbe_draw_string_centered(u16 y, const char *str, vbe_color_t fg,
			      vbe_color_t bg);
kernel_status_t vbe_draw_string_wrapped(u16 x, u16 y, u16 max_width,
					const char *str, vbe_color_t fg,
					vbe_color_t bg);

void vbe_show_info(void);
void vbe_list_modes(void);
kernel_status_t vbe_blit(u16 dst_x, u16 dst_y, u16 src_x, u16 src_y, u16 width,
			 u16 height);

#endif /* DRIVERS_VBE_H */