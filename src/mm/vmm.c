#include <drivers/vbe.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/vmm.h>
#include <printf.h>
#include <string.h>

page_directory_t kernel_page_directory;

#define PAGE_RECURSIVE_SLOT 1023
#define PAGE_RECURSIVE_PD 0xFFFFF000
#define PAGE_RECURSIVE_PT_BASE 0xFFC00000

static bool is_paging_enabled(void) {
  u32 cr0;
  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
  return (cr0 & 0x80000000) != 0;
}

kernel_status_t vmm_init(void) {
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
    kernel_status_t status =
        vmm_map_page(virt, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
    if (status != KERNEL_OK) {
      return status;
    }
  }

  if (vbe_is_available()) {
    vbe_device_t *dev = vbe_get_device();
    u32 fb_pages = ALIGN_UP(dev->framebuffer_size, PAGE_SIZE) / PAGE_SIZE;
    for (u32 i = 0; i < fb_pages; ++i) {
      u32 virt = dev->framebuffer_addr + i * PAGE_SIZE;
      vmm_map_page(virt, virt, PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
    }
  }

  kernel_page_directory[PAGE_RECURSIVE_SLOT] =
      (u32)&kernel_page_directory | PAGE_FLAG_PRESENT | PAGE_FLAG_RW;

  vmm_switch_directory(&kernel_page_directory);
  vmm_enable_paging();
  log(LOG_INFO, "VMM: paging enabled");
  return KERNEL_OK;
}

kernel_status_t vmm_map_page(u32 virt_addr, u32 phys_addr, u32 flags) {
  u32 pd_index = virt_addr >> 22;
  u32 pt_index = (virt_addr >> 12) & 0x3FF;

  if (is_paging_enabled()) {
    u32 *pd = (u32 *)PAGE_RECURSIVE_PD;
    if (!(pd[pd_index] & PAGE_FLAG_PRESENT)) {
      u32 pt_phys = pmm_alloc_page();
      if (!pt_phys)
        return KERNEL_OUT_OF_MEMORY;
      pd[pd_index] = pt_phys | PAGE_FLAG_PRESENT | PAGE_FLAG_RW;
      u32 *pt = (u32 *)(PAGE_RECURSIVE_PT_BASE + (pd_index << 12));
      memset(pt, 0, PAGE_SIZE);
    }
    u32 *pt = (u32 *)(PAGE_RECURSIVE_PT_BASE + (pd_index << 12));
    pt[pt_index] = (phys_addr & ~0xFFF) | flags;
  } else {
    if (!(kernel_page_directory[pd_index] & PAGE_FLAG_PRESENT)) {
      u32 pt_phys = pmm_alloc_page();
      if (!pt_phys)
        return KERNEL_OUT_OF_MEMORY;
      kernel_page_directory[pd_index] =
          pt_phys | PAGE_FLAG_PRESENT | PAGE_FLAG_RW;
      memset((void *)(kernel_page_directory[pd_index] & ~0xFFF), 0, PAGE_SIZE);
    }
    u32 *pt = (u32 *)(kernel_page_directory[pd_index] & ~0xFFF);
    pt[pt_index] = (phys_addr & ~0xFFF) | flags;
  }
  return KERNEL_OK;
}

kernel_status_t vmm_map_pages(u32 virt_addr, u32 phys_addr, u32 count,
                              u32 flags) {
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

kernel_status_t vmm_unmap_page(u32 virt_addr) {
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

kernel_status_t vmm_unmap_pages(u32 virt_addr, u32 count) {
  for (u32 i = 0; i < count; i++) {
    kernel_status_t status = vmm_unmap_page(virt_addr + i * PAGE_SIZE);
    if (status != KERNEL_OK) {
      return status;
    }
  }

  if (is_paging_enabled()) {
    u32 *pd = (u32 *)PAGE_RECURSIVE_PD;
    for (u32 i = 0; i < count; i++) {
      u32 pd_index = (virt_addr + i * PAGE_SIZE) >> 22;
      u32 *pt = (u32 *)(PAGE_RECURSIVE_PT_BASE + (pd_index << 12));
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
        __asm__ volatile("invlpg (%0)" : : "r"((u32)pt) : "memory");
      }
    }
  }
  return KERNEL_OK;
}

u32 vmm_get_physical_addr(u32 virt_addr) {
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

void vmm_switch_directory(page_directory_t *dir) {
  u32 dir_phys = (u32)dir;
  __asm__ volatile("mov %0, %%cr3" : : "r"(dir_phys));
}

void vmm_enable_paging(void) {
  __asm__ volatile("mov %%cr0, %%eax\n"
                   "or $0x80000000, %%eax\n"
                   "mov %%eax, %%cr0\n" ::
                       : "eax");
}