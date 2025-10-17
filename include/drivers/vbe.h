#ifndef DRIVERS_VBE_H
#define DRIVERS_VBE_H

#include <arch/i386/multiboot.h>
#include <kernel/kernel.h>

#define VBE_VERSION_1_0 0x0100
#define VBE_VERSION_1_1 0x0101
#define VBE_VERSION_1_2 0x0102
#define VBE_VERSION_2_0 0x0200
#define VBE_VERSION_3_0 0x0300

#define VBE_MODE_ATTR_SUPPORTED 0x0001      // Mode supported in hardware
#define VBE_MODE_ATTR_TTY 0x0004            // TTY output functions supported
#define VBE_MODE_ATTR_COLOR 0x0008          // Color mode
#define VBE_MODE_ATTR_GRAPHICS 0x0010       // Graphics mode
#define VBE_MODE_ATTR_VGA_COMPATIBLE 0x0020 // Not VGA compatible
#define VBE_MODE_ATTR_VGA_WINDOWED 0x0040   // VGA windowed mode not available
#define VBE_MODE_ATTR_LINEAR 0x0080         // Linear framebuffer mode available

#define VBE_MEMORY_MODEL_TEXT 0x00
#define VBE_MEMORY_MODEL_CGA 0x01
#define VBE_MEMORY_MODEL_HERCULES 0x02
#define VBE_MEMORY_MODEL_PLANAR 0x03
#define VBE_MEMORY_MODEL_PACKED_PIXEL 0x04
#define VBE_MEMORY_MODEL_NON_CHAIN4 0x05
#define VBE_MEMORY_MODEL_DIRECT_COLOR 0x06
#define VBE_MEMORY_MODEL_YUV 0x07

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

typedef struct {
  char signature[4];   // "VESA"
  u16 version;         // VBE version (0x0300 for VBE 3.0)
  u32 oem_string_ptr;  // Pointer to OEM string
  u32 capabilities;    // Capabilities of graphics controller
  u32 video_modes_ptr; // Pointer to list of supported video modes
  u16 total_memory;    // Total memory on card in 64KB blocks

  // VBE 2.0+ fields
  u16 oem_software_rev;     // OEM software revision
  u32 oem_vendor_name_ptr;  // Pointer to vendor name string
  u32 oem_product_name_ptr; // Pointer to product name string
  u32 oem_product_rev_ptr;  // Pointer to product revision string
  u8 reserved[222];         // Reserved for VBE implementation scratch area
  u8 oem_data[256];         // Data area for OEM strings
} __attribute__((packed)) vbe_control_info_t;

// VBE Mode Information
typedef struct {
  // Mandatory information for all VBE revisions
  u16 mode_attributes;    // Mode attributes
  u8 win_a_attributes;    // Window A attributes
  u8 win_b_attributes;    // Window B attributes
  u16 win_granularity;    // Window granularity
  u16 win_size;           // Window size
  u16 win_a_segment;      // Window A start segment
  u16 win_b_segment;      // Window B start segment
  u32 win_func_ptr;       // Pointer to window function
  u16 bytes_per_scanline; // Bytes per scan line

  // Mandatory information for VBE 1.2 and above
  u16 x_resolution;         // Horizontal resolution in pixels/characters
  u16 y_resolution;         // Vertical resolution in pixels/characters
  u8 x_char_size;           // Character cell width in pixels
  u8 y_char_size;           // Character cell height in pixels
  u8 number_of_planes;      // Number of memory planes
  u8 bits_per_pixel;        // Bits per pixel
  u8 number_of_banks;       // Number of banks
  u8 memory_model;          // Memory model type
  u8 bank_size;             // Bank size in KB
  u8 number_of_image_pages; // Number of images
  u8 reserved_1;            // Reserved for page function

  // Direct Color fields (required for direct/6 and YUV/7 memory models)
  u8 red_mask_size;          // Size of direct color red mask in bits
  u8 red_field_position;     // Bit position of lsb of red mask
  u8 green_mask_size;        // Size of direct color green mask in bits
  u8 green_field_position;   // Bit position of lsb of green mask
  u8 blue_mask_size;         // Size of direct color blue mask in bits
  u8 blue_field_position;    // Bit position of lsb of blue mask
  u8 rsvd_mask_size;         // Size of direct color reserved mask in bits
  u8 rsvd_field_position;    // Bit position of lsb of reserved mask
  u8 direct_color_mode_info; // Direct color mode attributes

  // Mandatory information for VBE 2.0 and above
  u32 phys_base_ptr; // Physical address for flat memory model
  u32 reserved_2;    // Reserved - always set to 0
  u16 reserved_3;    // Reserved - always set to 0

  // Mandatory information for VBE 3.0 and above
  u16 lin_bytes_per_scan_line;     // Bytes per scan line for linear modes
  u8 banked_number_of_image_pages; // Number of images for banked modes
  u8 lin_number_of_image_pages;    // Number of images for linear modes
  u8 lin_red_mask_size;            // Size of direct color red mask (linear)
  u8 lin_red_field_position;       // Bit position of lsb of red mask (linear)
  u8 lin_green_mask_size;          // Size of direct color green mask (linear)
  u8 lin_green_field_position;     // Bit position of lsb of green mask (linear)
  u8 lin_blue_mask_size;           // Size of direct color blue mask (linear)
  u8 lin_blue_field_position;      // Bit position of lsb of blue mask (linear)
  u8 lin_rsvd_mask_size;      // Size of direct color reserved mask (linear)
  u8 lin_rsvd_field_position; // Bit position of lsb of reserved mask (linear)
  u32 max_pixel_clock;        // Maximum pixel clock (in Hz) for graphics mode
  u8 reserved_4[189];         // Reserved for VBE implementation
} __attribute__((packed)) vbe_mode_info_t;

typedef struct {
  bool initialized;      // Is VBE initialized?
  bool linear_supported; // Linear framebuffer available?
  u16 current_mode;      // Current VBE mode
  u32 framebuffer_addr;  // Physical address of framebuffer
  u32 framebuffer_size;  // Size of framebuffer in bytes
  u16 width;             // Screen width in pixels
  u16 height;            // Screen height in pixels
  u8 bpp;                // Bits per pixel
  u16 pitch;             // Bytes per scanline
  u8 memory_model;       // Memory model

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

typedef struct {
  u8 red;
  u8 green;
  u8 blue;
  u8 alpha;
} vbe_color_t;

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
kernel_status_t vbe_scroll();
kernel_status_t vbe_draw_filled_rect(u16 x, u16 y, u16 width, u16 height,
                                     vbe_color_t fill, vbe_color_t border);
void vbe_draw_string(u16 x, u16 y, const char *str, vbe_color_t fg,
                     vbe_color_t bg);
void vbe_draw_string_centered(u16 y, const char *str, vbe_color_t fg,
                              vbe_color_t bg);
kernel_status_t vbe_draw_string_wrapped(u16 x, u16 y, u16 max_width,
                                        const char *str, vbe_color_t fg,
                                        vbe_color_t bg);

kernel_status_t vbe_show_info(void);
kernel_status_t vbe_list_modes(void);
kernel_status_t vbe_blit(u16 dst_x, u16 dst_y, u16 src_x, u16 src_y, u16 width,
                         u16 height);

#endif // DRIVERS_VBE_H