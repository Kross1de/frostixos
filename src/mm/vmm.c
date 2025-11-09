#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/vmm.h>
#include <printf.h>
#include <string.h>

page_directory_t kernel_page_directory;

#define PAGE_RECURSIVE_SLOT 1023
#define PAGE_RECURSIVE_PD 0xFFFFF000
#define PAGE_RECURSIVE_PT_BASE 0xFFC00000

/* Invalidate a single page in the TLB for virtual address va */
static inline void tlb_invlpg(u32 va)
{
	__asm__ volatile("invlpg (%0)" ::"r"(va) : "memory");
}

/* Returns true if paging is currently enabled (CR0.PG set) */
static bool is_paging_enabled(void)
{
	u32 cr0;
	__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
	return (cr0 & 0x80000000) != 0;
}

kernel_status_t vmm_init(void)
{
	memset(kernel_page_directory, 0, sizeof(page_directory_t));

	extern u32 _kernel_end;
	u32 kernel_start = 0x00100000;
	u32 kernel_size = (u32)&_kernel_end - kernel_start;
	u32 num_pages = ALIGN_UP(kernel_size, PAGE_SIZE) / PAGE_SIZE;

	for (u32 i = 0; i < num_pages; ++i) {
		u32 virt = kernel_start + i * PAGE_SIZE;
		u32 phys = virt;
		vmm_map_page(virt, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
	}

	u32 bitmap_start = (u32)g_physical_allocator.bits;
	u32 bitmap_size = g_physical_allocator.size;
	u32 bitmap_pages = ALIGN_UP(bitmap_size, PAGE_SIZE) / PAGE_SIZE;

	for (u32 i = 0; i < bitmap_pages; ++i) {
		u32 virt = bitmap_start + i * PAGE_SIZE;
		u32 phys = virt;
		kernel_status_t status = vmm_map_page(
			virt, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
		if (status != KERNEL_OK) {
			return status;
		}
	}

	if (vbe_is_available()) {
		vbe_device_t *dev = vbe_get_device();
		u32 fb_pages = ALIGN_UP(dev->framebuffer_size, PAGE_SIZE)
			       / PAGE_SIZE;
		for (u32 i = 0; i < fb_pages; ++i) {
			u32 virt = dev->framebuffer_addr + i * PAGE_SIZE;
			vmm_map_page(virt, virt,
				     PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
		}
	}

	u32 low_pt_phys;
	if (!(kernel_page_directory[0] & PAGE_FLAG_PRESENT)) {
		low_pt_phys = pmm_alloc_page();
		if (!low_pt_phys) return KERNEL_OUT_OF_MEMORY;
		if (low_pt_phys == 0)
			return KERNEL_OUT_OF_MEMORY;
		kernel_page_directory[0]
			= low_pt_phys | PAGE_FLAG_PRESENT | PAGE_FLAG_RW;
		memset((void *)low_pt_phys, 0, PAGE_SIZE);
	} else {
		low_pt_phys = kernel_page_directory[0] & ~0xFFF;
	}

	u32 *low_pt = (u32 *)low_pt_phys;
	for (u32 i = 0; i < 256; i++) {
		if (!(low_pt[i] & PAGE_FLAG_PRESENT)) {
			low_pt[i] = (i * PAGE_SIZE) | PAGE_FLAG_PRESENT
				    | PAGE_FLAG_RW;
		}
	}

	extern u32 _multiboot_info_ptr;
	multiboot_info_t *mbi = (multiboot_info_t *)_multiboot_info_ptr;
	if (mbi && (mbi->flags & (1 << 11))) {
		u32 mbi_addr = (u32)mbi;
		u32 mbi_pages = ALIGN_UP(sizeof(multiboot_info_t), PAGE_SIZE)
				/ PAGE_SIZE;
		for (u32 i = 0; i < mbi_pages; ++i) {
			u32 virt = mbi_addr + i * PAGE_SIZE;
			vmm_map_page(virt, virt,
				     PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
		}

		u32 vbe_control_addr = mbi->vbe_control_info;
		u32 vbe_control_pages
			= ALIGN_UP(sizeof(vbe_control_info_t), PAGE_SIZE)
			  / PAGE_SIZE;
		for (u32 i = 0; i < vbe_control_pages; ++i) {
			u32 virt = vbe_control_addr + i * PAGE_SIZE;
			vmm_map_page(virt, virt,
				     PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
		}

		u32 vbe_mode_addr = mbi->vbe_mode_info;
		u32 vbe_mode_pages
			= ALIGN_UP(sizeof(vbe_mode_info_t), PAGE_SIZE)
			  / PAGE_SIZE;
		for (u32 i = 0; i < vbe_mode_pages; ++i) {
			u32 virt = vbe_mode_addr + i * PAGE_SIZE;
			vmm_map_page(virt, virt,
				     PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
		}

		vbe_device_t *dev = vbe_get_device();
		u32 video_modes_addr = dev->control_info.video_modes_ptr;
		u32 seg = video_modes_addr >> 16;
		u32 off = video_modes_addr & 0xFFFF;
		video_modes_addr = seg * 16 + off;
		vmm_map_page(ALIGN_DOWN(video_modes_addr, PAGE_SIZE),
			     ALIGN_DOWN(video_modes_addr, PAGE_SIZE),
			     PAGE_FLAG_PRESENT | PAGE_FLAG_RW);

		u32 oem_string_addr = dev->control_info.oem_string_ptr;
		vmm_map_page(ALIGN_DOWN(oem_string_addr, PAGE_SIZE),
			     ALIGN_DOWN(oem_string_addr, PAGE_SIZE),
			     PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
	}

	kernel_page_directory[PAGE_RECURSIVE_SLOT] = (u32)&kernel_page_directory
						     | PAGE_FLAG_PRESENT
						     | PAGE_FLAG_RW;

	vmm_switch_directory(&kernel_page_directory);
	vmm_enable_paging();
	log(LOG_OKAY, "VMM: paging enabled");
	return KERNEL_OK;
}

kernel_status_t vmm_map_page(u32 virt_addr, u32 phys_addr, u32 flags)
{
	u32 pd_index = virt_addr >> 22;
	u32 pt_index = (virt_addr >> 12) & 0x3FF;

	if (is_paging_enabled()) {
		u32 *pd = (u32 *)PAGE_RECURSIVE_PD;
		if (!(pd[pd_index] & PAGE_FLAG_PRESENT)) {
			u32 pt_phys = pmm_alloc_page();
			if (!pt_phys)
				return KERNEL_OUT_OF_MEMORY;
			tlb_invlpg((u32)(PAGE_RECURSIVE_PT_BASE + (pd_index << 12)));
			pd[pd_index] = (pt_phys & ~0xFFF) | PAGE_FLAG_PRESENT
				       | PAGE_FLAG_RW;
			memset((void *)(PAGE_RECURSIVE_PT_BASE
					+ (pd_index << 12)),
			       0, PAGE_SIZE);
		}
		u32 *pt = (u32 *)(PAGE_RECURSIVE_PT_BASE + (pd_index << 12));
		u32 newpte = (phys_addr & ~0xFFF) | (flags & PTE_FLAGS_MASK);
		if ((pt[pt_index] & PAGE_FLAG_PRESENT)
		    && ((pt[pt_index] & ~0xFFF) != (phys_addr & ~0xFFF))) {
			return KERNEL_ALREADY_MAPPED;
		}
		pt[pt_index] = newpte;
		tlb_invlpg(virt_addr);
	} else {
		if (!(kernel_page_directory[pd_index] & PAGE_FLAG_PRESENT)) {
			u32 pt_phys = pmm_alloc_page();
			if (!pt_phys)
				return KERNEL_OUT_OF_MEMORY;
			kernel_page_directory[pd_index] = (pt_phys & ~0xFFF)
							  | PAGE_FLAG_PRESENT
							  | PAGE_FLAG_RW;
			memset((void *)(pt_phys), 0, PAGE_SIZE);
		}
		u32 *pt = (u32 *)(kernel_page_directory[pd_index] & ~0xFFF);
		pt[pt_index] = (phys_addr & ~0xFFF) | (flags & PTE_FLAGS_MASK);
	}
	return KERNEL_OK;
}

kernel_status_t vmm_map_pages(u32 virt_addr, u32 phys_addr, u32 count,
			      u32 flags)
{
	for (u32 i = 0; i < count; i++) {
		u32 v = virt_addr + i * PAGE_SIZE;
		u32 p = phys_addr + i * PAGE_SIZE;
		kernel_status_t status = vmm_map_page(v, p, flags);
		if (status != KERNEL_OK) {
			for (u32 j = 0; j < i; j++) {
				vmm_unmap_page(virt_addr + j * PAGE_SIZE);
			}
			return status;
		}
	}
	return KERNEL_OK;
}

/* Convenience: map a page-aligned range of bytes; requires page-aligned
 * virt_start and phys_start. Returns KERNEL_INVALID_PARAM if inputs are not
 * page-aligned.
 */
kernel_status_t vmm_map_range(u32 virt_start, u32 phys_start, u32 size,
			      u32 flags)
{
	if ((virt_start & (PAGE_SIZE - 1)) || (phys_start & (PAGE_SIZE - 1)))
		return KERNEL_INVALID_PARAM;
	if (size == 0)
		return KERNEL_INVALID_PARAM;

	u32 start = virt_start;
	u32 end = ALIGN_UP(virt_start + size, PAGE_SIZE);
	u32 pages = (end - start) / PAGE_SIZE;
	return vmm_map_pages(start, phys_start, pages, flags);
}

/* Check if all pages in the virtual range are mapped. Returns true if every
 * page in [virt_start, virt_start+size) has a present PTE.
 */
bool vmm_is_range_mapped(u32 virt_start, u32 size)
{
	if (size == 0)
		return false;

	u32 start = ALIGN_DOWN(virt_start, PAGE_SIZE);
	u32 end = ALIGN_UP(virt_start + size, PAGE_SIZE);
	for (u32 addr = start; addr < end; addr += PAGE_SIZE) {
		u32 phys = vmm_get_physical_addr(addr);
		if (phys == 0)
			return false;
	}
	return true;
}

kernel_status_t vmm_unmap_page(u32 virt_addr)
{
	u32 pd_index = virt_addr >> 22;
	u32 pt_index = (virt_addr >> 12) & 0x3FF;

	if (is_paging_enabled()) {
		u32 *pd = (u32 *)PAGE_RECURSIVE_PD;
		if (!(pd[pd_index] & PAGE_FLAG_PRESENT)) {
			return KERNEL_INVALID_PARAM;
		}
		u32 *pt = (u32 *)(PAGE_RECURSIVE_PT_BASE + (pd_index << 12));
		if (!(pt[pt_index] & PAGE_FLAG_PRESENT)) {
			return KERNEL_INVALID_PARAM;
		}
		pt[pt_index] = 0;
	} else {
		if (!(kernel_page_directory[pd_index] & PAGE_FLAG_PRESENT)) {
			return KERNEL_INVALID_PARAM;
		}
		u32 *pt = (u32 *)(kernel_page_directory[pd_index] & ~0xFFF);
		if (!(pt[pt_index] & PAGE_FLAG_PRESENT)) {
			return KERNEL_INVALID_PARAM;
		}
		pt[pt_index] = 0;
	}

	__asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");

	return KERNEL_OK;
}

kernel_status_t vmm_unmap_pages(u32 virt_addr, u32 count)
{
	for (u32 i = 0; i < count; i++) {
		kernel_status_t status
			= vmm_unmap_page(virt_addr + i * PAGE_SIZE);
		if (status != KERNEL_OK) {
			return status;
		}
	}

	if (is_paging_enabled()) {
		u32 *pd = (u32 *)PAGE_RECURSIVE_PD;
		for (u32 i = 0; i < count; i++) {
			u32 pd_index = (virt_addr + i * PAGE_SIZE) >> 22;
			u32 *pt = (u32 *)(PAGE_RECURSIVE_PT_BASE
					  + (pd_index << 12));
			bool empty = true;
			for (u32 j = 0; j < PAGE_TABLE_ENTRIES; j++) {
				if (pt[j] & PAGE_FLAG_PRESENT) {
					empty = false;
					break;
				}
			}
			if (empty && (pd[pd_index] & PAGE_FLAG_PRESENT)) {
				u32 pt_phys = pd[pd_index] & ~0xFFF;
				pd[pd_index] = 0;
				pmm_free_page(pt_phys);
				__asm__ volatile("invlpg (%0)"
						 :
						 : "r"((u32)pt)
						 : "memory");
			}
		}
	}
	return KERNEL_OK;
}

kernel_status_t vmm_map_if_not_mapped(u32 phys_start, u32 size)
{
	u32 start = ALIGN_DOWN(phys_start, PAGE_SIZE);
	u32 end = ALIGN_UP(phys_start + size, PAGE_SIZE);
	u32 num_pages = (end - start) / PAGE_SIZE;

	for (u32 i = 0; i < num_pages; i++) {
		u32 va = start + i * PAGE_SIZE;
		u32 pa = va;

		u32 current_phys = vmm_get_physical_addr(va);
		if (current_phys == 0) {
			kernel_status_t status
				= vmm_map_page(va, pa,
					       PAGE_FLAG_PRESENT | PAGE_FLAG_RW
						       | PAGE_FLAG_GLOBAL);
			if (status != KERNEL_OK)
				return status;
		} else if (current_phys != pa) {
			log(LOG_ERR,
			    "VMM: Mapping conflict at virt 0x%x (mapped to "
			    "phys 0x%x, expected "
			    "0x%x)",
			    va, current_phys, pa);
			return KERNEL_INVALID_PARAM;
		}
	}
	return KERNEL_OK;
}

u32 vmm_get_physical_addr(u32 virt_addr)
{
	u32 pd_index = virt_addr >> 22;
	u32 pt_index = (virt_addr >> 12) & 0x3FF;

	if (is_paging_enabled()) {
		u32 *pd = (u32 *)PAGE_RECURSIVE_PD;
		if (!(pd[pd_index] & PAGE_FLAG_PRESENT)) {
			return 0;
		}
		u32 *pt = (u32 *)(PAGE_RECURSIVE_PT_BASE + (pd_index << 12));
		if (!(pt[pt_index] & PAGE_FLAG_PRESENT)) {
			return 0;
		}
		return (pt[pt_index] & ~0xFFF) | (virt_addr & 0xFFF);
	} else {
		if (!(kernel_page_directory[pd_index] & PAGE_FLAG_PRESENT)) {
			return 0;
		}
		u32 *pt = (u32 *)(kernel_page_directory[pd_index] & ~0xFFF);
		if (!(pt[pt_index] & PAGE_FLAG_PRESENT)) {
			return 0;
		}
		return (pt[pt_index] & ~0xFFF) | (virt_addr & 0xFFF);
	}
}

void vmm_switch_directory(page_directory_t *dir)
{
	u32 dir_phys = (u32)dir;
	__asm__ volatile("mov %0, %%cr3" : : "r"(dir_phys));
}

void vmm_enable_paging(void)
{
	__asm__ volatile("mov %%cr0, %%eax\n"
			 "or $0x80000000, %%eax\n"
			 "mov %%eax, %%cr0\n" ::
				 : "eax");
}