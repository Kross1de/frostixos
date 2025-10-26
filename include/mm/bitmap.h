#ifndef MM_BITMAP_H
#define MM_BITMAP_H

#include <arch/i386/multiboot.h>
#include <kernel/kernel.h>

/*
 * Simple physical memory bitmap allocator representation and API.
 * g_physical_allocator holds global allocator metadata.
 */
typedef struct {
	u8 *bits;		/* bitmap bit array */
	u32 size;		/* size of bitmap in bytes */
	u32 total_pages;	/* total managed pages */
	u32 free_pages;		/* free pages available */
	u32 used_pages;		/* used pages */
} bitmap_allocator_t;

extern bitmap_allocator_t g_physical_allocator;

/* Physical memory manager (PMM) API */
kernel_status_t pmm_init(multiboot_info_t *mb_info);
u32 pmm_alloc_page(void);
void pmm_free_page(u32 addr);
u32 pmm_get_total_pages(void);
u32 pmm_get_free_pages(void);
u32 pmm_alloc_pages(u32 count);
void pmm_free_pages(u32 addr, u32 count);

#endif /* MM_BITMAP_H */