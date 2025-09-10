#ifndef ARCH_I386_MULTIBOOT_H
#define ARCH_I386_MULTIBOOT_H

#include <kernel/kernel.h>

#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MULTIBOOT_PAGE_ALIGN 0x00000001
#define MULTIBOOT_MEMORY_INFO 0x00000002
#define MULTIBOOT_VIDEO_MODE 0x00000004
#define MULTIBOOT_AOUT_KLUDGE 0x00010000
#define MULTIBOOT_INFO_MODS 0x00000008
#define MULTIBOOT_INFO_MEM_MAP 0x00000040

#define MULTIBOOT_FLAGS                                                        \
  (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE)

#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2

typedef struct {
  u32 magic;
  u32 flags;
  u32 checksum;

  u32 header_addr;
  u32 load_addr;
  u32 load_end_addr;
  u32 bss_end_addr;
  u32 entry_addr;

  u32 mode_type;
  u32 width;
  u32 height;
  u32 depth;
} __attribute__((packed)) multiboot_header_t;

typedef struct {
  u32 size;
  u64 addr;
  u64 len;
  u32 type;
} __attribute__((packed)) multiboot_memory_map_t;

typedef struct {
  u32 mod_start;
  u32 mod_end;
  u32 string;
  u32 reserved;
} __attribute__((packed)) multiboot_module_t;

typedef struct {
  char signature[4];
  u16 version;
  u32 oem_string_ptr;
  u32 capabilities;
  u32 video_modes_ptr;
  u16 total_memory;
  u16 oem_software_rev;
  u32 oem_vendor_name_ptr;
  u32 oem_product_name_ptr;
  u32 oem_product_rev_ptr;
  u8 reserved[222];
  u8 oem_data[256];
} __attribute__((packed)) multiboot_vbe_info_t;

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
  u8 reserved1;
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
  u32 reserved2;
  u16 reserved3;
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
  u8 reserved4[189];
} __attribute__((packed)) multiboot_vbe_mode_info_t;

typedef struct {
  u32 flags;
  u32 mem_lower;
  u32 mem_upper;
  u32 boot_device;
  u32 cmdline;
  u32 mods_count;
  u32 mods_addr;

  union {
    struct {
      u32 tabsize;
      u32 strsize;
      u32 addr;
      u32 reserved;
    } aout_sym;

    struct {
      u32 num;
      u32 size;
      u32 addr;
      u32 shndx;
    } elf_sec;
  } u;

  u32 mmap_length;
  u32 mmap_addr;
  u32 drives_length;
  u32 drives_addr;
  u32 config_table;
  u32 boot_loader_name;
  u32 apm_table;
  u32 vbe_control_info; // phys addr of VBE control info
  u32 vbe_mode_info;    // phys addr of VBE mode info
  u16 vbe_mode;
  u16 vbe_interface_seg;
  u16 vbe_interface_off;
  u16 vbe_interface_len;
  u64 framebuffer_addr;
  u32 framebuffer_pitch;
  u32 framebuffer_width;
  u32 framebuffer_height;
  u8 framebuffer_bpp;
  u8 framebuffer_type;
  union {
    struct {
      u32 framebuffer_palette_addr;
      u16 framebuffer_palette_num_colors;
    };
    struct {
      u8 framebuffer_red_field_position;
      u8 framebuffer_red_mask_size;
      u8 framebuffer_green_field_position;
      u8 framebuffer_green_mask_size;
      u8 framebuffer_blue_field_position;
      u8 framebuffer_blue_mask_size;
    };
  };
} __attribute__((packed)) multiboot_info_t;

kernel_status_t multiboot_init(u32 magic, multiboot_info_t *mbi);
u32 multiboot_get_memory_size(void);
multiboot_info_t *multiboot_get_info(void);

#endif // ARCH_I386_MULTIBOOT_H
