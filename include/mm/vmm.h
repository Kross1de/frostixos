#ifndef MM_VMM_H
#define MM_VMM_H

#include <kernel/kernel.h>
#include <stdint.h>

#define PAGE_DIR_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024
#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_RW (1 << 1)
#define PAGE_FLAG_USER (1 << 2)

typedef u32 page_directory_t[PAGE_DIR_ENTRIES] __attribute__((aligned(4096)));
typedef u32 page_table_t[PAGE_TABLE_ENTRIES] __attribute__((aligned(4096)));

extern page_directory_t kernel_page_directory;

kernel_status_t vmm_init(void);
kernel_status_t vmm_map_page(u32 virt_addr, u32 phys_addr, u32 flags);
kernel_status_t vmm_unmap_page(u32 virt_addr);
u32 vmm_get_physical_addr(u32 virt_addr);
void vmm_switch_directory(page_directory_t *dir);
void vmm_enable_paging(void);

#endif
