#include <arch/i386/multiboot.h>
#include <drivers/serial.h>
#include <kernel/kernel.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <printf.h>
#include <string.h>

bitmap_allocator_t g_physical_allocator = {0};

/* Set a bit in the physical allocation bitmap (no counters here) */
static void bitmap_set_bit(u32 bit)
{
	if (bit / 8 >= g_physical_allocator.size)
		return;
	g_physical_allocator.bits[bit / 8] |= (1 << (bit % 8));
}

/* Clear a bit in the physical allocation bitmap (no counters here) */
static void bitmap_clear_bit(u32 bit)
{
	if (bit / 8 >= g_physical_allocator.size)
		return;
	g_physical_allocator.bits[bit / 8] &= ~(1 << (bit % 8));
}

/* Return true if bit is set (page allocated) */
static bool bitmap_test_bit(u32 bit)
{
	if (bit / 8 >= g_physical_allocator.size)
		return false;
	return (g_physical_allocator.bits[bit / 8] & (1 << (bit % 8))) != 0;
}

static u32 bitmap_find_free_bit(void)
{
	for (u32 byte = 0; byte < g_physical_allocator.size; byte++) {
		if (g_physical_allocator.bits[byte] != 0xFF) {
			for (u32 bit = 0; bit < 8; bit++) {
				if (!(g_physical_allocator.bits[byte] & (1 << bit))) {
					return (byte * 8) + bit;
				}
			}
		}
	}
	return (u32)-1;
}

static u32 bitmap_find_free_bits(u32 count)
{
	if (count == 0)
		return (u32)-1;

	u32 found = 0;
	for (u32 byte = 0; byte < g_physical_allocator.size; byte++) {
		if (g_physical_allocator.bits[byte] == 0xFF) {
			found = 0;
			continue;
		}

		for (u32 bit = 0; bit < 8; bit++) {
			u32 idx = byte * 8 + bit;
			if (bitmap_test_bit(idx)) {
				found = 0;
				continue;
			}
			found++;
			if (found == count) {
				return (byte * 8 + bit + 1 - count);
			}
		}
	}
	return (u32)-1;
}

/* Mark a physical byte range as allocated (adjust counters safely) */
static void mark_region_allocated(u64 start, u64 len)
{
	if (len == 0)
		return;

	u32 start_page = (u32)(start / PAGE_SIZE);
	u32 end_page = (u32)((start + len + PAGE_SIZE - 1) / PAGE_SIZE);

	for (u32 page = start_page;
	     page < end_page && page < g_physical_allocator.total_pages;
	     page++) {
		if (!bitmap_test_bit(page)) {
			bitmap_set_bit(page);
			/* adjust counters safely */
			g_physical_allocator.used_pages++;
			if (g_physical_allocator.free_pages > 0)
				g_physical_allocator.free_pages--;
		}
	}
}

/* Mark a physical byte range as free (adjust counters safely) */
static void __attribute__((unused)) mark_region_free(u64 start, u64 len)
{
	if (len == 0)
		return;

	u32 start_page = (u32)(ALIGN_UP(start, PAGE_SIZE) / PAGE_SIZE);
	u32 end_page = (u32)((start + len) / PAGE_SIZE);

	for (u32 page = start_page;
	     page < end_page && page < g_physical_allocator.total_pages;
	     page++) {
		if (bitmap_test_bit(page)) {
			bitmap_clear_bit(page);
			g_physical_allocator.free_pages++;
			if (g_physical_allocator.used_pages > 0)
				g_physical_allocator.used_pages--;
		}
	}
}

kernel_status_t pmm_init(multiboot_info_t *mb_info)
{
	if (!(mb_info->flags & MULTIBOOT_INFO_MEM_MAP)) {
		return KERNEL_ERROR;
	}

	u64 max_addr = 0;
	multiboot_memory_map_t *mmap
		= (multiboot_memory_map_t *)mb_info->mmap_addr;
	while ((u32)mmap < mb_info->mmap_addr + mb_info->mmap_length) {
		if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
			u64 end = mmap->addr + mmap->len;
			if (end > max_addr)
				max_addr = end;
		}
		mmap = (multiboot_memory_map_t *)((u32)mmap + mmap->size
						  + sizeof(mmap->size));
	}

	/* total managed pages */
	g_physical_allocator.total_pages = (u32)(max_addr / PAGE_SIZE);

	/* bitmap size in bytes and pages */
	u32 bitmap_bytes = (g_physical_allocator.total_pages + 7) / 8;
	g_physical_allocator.size = (u32)ALIGN_UP(bitmap_bytes, PAGE_SIZE);

	extern u32 _kernel_end;
	u32 bitmap_addr = PAGE_ALIGN((u32)&_kernel_end);
	g_physical_allocator.bits = (u8 *)bitmap_addr;

	/* mark all pages allocated initially (we'll free the available ones) */
	memset(g_physical_allocator.bits, 0xFF, g_physical_allocator.size);
	g_physical_allocator.free_pages = 0;
	g_physical_allocator.used_pages = g_physical_allocator.total_pages;

	/* Reserve low memory and the space used by the bitmap itself so clearing
	 * available regions won't touch them.
	 */
	mark_region_allocated(0, 0x100000);
	mark_region_allocated(bitmap_addr, g_physical_allocator.size);

	/* Now walk the memory map and mark available pages (>= 1MB) as free,
	 * but do not touch pages we've already reserved above.
	 */
	mmap = (multiboot_memory_map_t *)mb_info->mmap_addr;
	while ((u32)mmap < mb_info->mmap_addr + mb_info->mmap_length) {
		if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE && mmap->addr >= 0x100000) {
			u32 start_page = (u32)(ALIGN_UP(mmap->addr, PAGE_SIZE) / PAGE_SIZE);
			u32 end_page = (u32)((mmap->addr + mmap->len) / PAGE_SIZE);
			for (u32 page = start_page; page < end_page; page++) {
				u32 page_addr = page * PAGE_SIZE;
				/* do not free pages that belong to the bitmap area */
				if (page_addr >= bitmap_addr
				    && page_addr < bitmap_addr + g_physical_allocator.size) {
					continue;
				}
				if (bitmap_test_bit(page)) {
					bitmap_clear_bit(page);
					g_physical_allocator.free_pages++;
					if (g_physical_allocator.used_pages > 0)
						g_physical_allocator.used_pages--;
				}
			}
		}
		mmap = (multiboot_memory_map_t *)((u32)mmap + mmap->size
						  + sizeof(mmap->size));
	}

	/* Reserve boot/multiboot structures and modules as used */
	mark_region_allocated((u32)mb_info, sizeof(multiboot_info_t));
	mark_region_allocated(mb_info->mmap_addr, mb_info->mmap_length);
	if (mb_info->flags & MULTIBOOT_INFO_MODS) {
		multiboot_module_t *mod
			= (multiboot_module_t *)mb_info->mods_addr;
		for (u32 i = 0; i < mb_info->mods_count; i++) {
			mark_region_allocated(mod->mod_start,
					      mod->mod_end - mod->mod_start);
			mod++;
		}
	}

	/* Reserve the kernel region itself (if not already reserved) */
	mark_region_allocated(0x100000, (u32)&_kernel_end - 0x100000);

	log(LOG_OKAY, "PMM initialized: %u total pages, %u free pages",
	    g_physical_allocator.total_pages, g_physical_allocator.free_pages);
	return KERNEL_OK;
}

u32 pmm_alloc_page(void)
{
	if (g_physical_allocator.free_pages == 0) {
		return 0;
	}
	u32 bit = bitmap_find_free_bit();
	if (bit == (u32)-1) {
		return 0;
	}
	bitmap_set_bit(bit);
	g_physical_allocator.free_pages--;
	g_physical_allocator.used_pages++;
	return bit * PAGE_SIZE;
}

u32 pmm_alloc_pages(u32 count)
{
	if (g_physical_allocator.free_pages < count) {
		return 0;
	}
	u32 start_bit = bitmap_find_free_bits(count);
	if (start_bit == (u32)-1) {
		return 0;
	}
	for (u32 i = 0; i < count; i++) {
		bitmap_set_bit(start_bit + i);
	}
	g_physical_allocator.free_pages -= count;
	g_physical_allocator.used_pages += count;
	return start_bit * PAGE_SIZE;
}

void pmm_free_page(u32 addr)
{
	if (addr == 0)
		return;
	u32 bit = addr / PAGE_SIZE;
	if (bitmap_test_bit(bit)) {
		bitmap_clear_bit(bit);
		g_physical_allocator.free_pages++;
		if (g_physical_allocator.used_pages > 0)
			g_physical_allocator.used_pages--;
	}
}

void pmm_free_pages(u32 addr, u32 count)
{
	if (addr == 0 || count == 0)
		return;

	u32 start_bit = addr / PAGE_SIZE;
	for (u32 i = 0; i < count; i++) {
		u32 bit = start_bit + i;
		if (bitmap_test_bit(bit)) {
			bitmap_clear_bit(bit);
			g_physical_allocator.free_pages++;
			if (g_physical_allocator.used_pages > 0)
				g_physical_allocator.used_pages--;
		}
	}
}

u32 pmm_get_total_pages(void)
{
	return g_physical_allocator.total_pages;
}

u32 pmm_get_free_pages(void)
{
	return g_physical_allocator.free_pages;
}