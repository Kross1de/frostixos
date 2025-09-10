#ifndef MM_BITMAP_H
#define MM_BITMAP_H

#include <arch/i386/multiboot.h>
#include <kernel/kernel.h>

typedef struct {
  u8 *bits;
  u32 size;
  u32 total_pages;
  u32 free_pages;
  u32 used_pages;
} bitmap_allocator_t;

extern bitmap_allocator_t g_physical_allocator;

kernel_status_t pmm_init(multiboot_info_t *mb_info);
u32 pmm_alloc_page(void);
void pmm_free_page(u32 addr);
u32 pmm_get_total_pages(void);
u32 pmm_get_free_pages(void);

#endif // MM_BITMAP_H
