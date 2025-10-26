#ifndef MM_VMM_H
#define MM_VMM_H

#include <kernel/kernel.h>
#include <stdint.h>

/*
 * 32-bit paging helpers and types.
 *
 * This header provides page table and directory constants and function
 * prototypes for mapping/unmapping physical pages into the virtual address
 * space.
 */
#define PAGE_DIR_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024
#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_RW (1 << 1)
#define PAGE_FLAG_USER (1 << 2)
#define PAGE_FLAG_GLOBAL (1 << 8)

#ifndef PTE_FLAGS_MASK
#define PTE_FLAGS_MASK (PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_USER)
#endif

/* Page directory and table types aligned to 4K for hardware requirements */
typedef u32 page_directory_t[PAGE_DIR_ENTRIES] __attribute__((aligned(4096)));
typedef u32 page_table_t[PAGE_TABLE_ENTRIES] __attribute__((aligned(4096)));

/* The kernel page directory instance used by the kernel mapping helpers */
extern page_directory_t kernel_page_directory;

/* VMM API */
kernel_status_t vmm_init(void);
kernel_status_t vmm_map_page(u32 virt_addr, u32 phys_addr, u32 flags);
kernel_status_t vmm_unmap_page(u32 virt_addr);
kernel_status_t vmm_map_pages(u32 virt_addr, u32 phys_addr, u32 count,
			      u32 flags);
kernel_status_t vmm_unmap_pages(u32 virt_addr, u32 count);
kernel_status_t vmm_map_if_not_mapped(u32 phys_start, u32 size);
u32 vmm_get_physical_addr(u32 virt_addr);
void vmm_switch_directory(page_directory_t *dir);
void vmm_enable_paging(void);

/* New helpers */
kernel_status_t vmm_map_range(u32 virt_start, u32 phys_start, u32 size,
			      u32 flags);
bool vmm_is_range_mapped(u32 virt_start, u32 size);

#endif /* MM_VMM_H */