#ifndef MM_VMA_H
#define MM_VMA_H

#include <kernel/kernel.h>
#include <mm/vmm.h>

/*
 * Virtual Memory Area (VMA) and mm_struct management.
 *
 * This header declares structures and helpers for representing ranges of
 * virtual address space that are backed by memory mappings.
 */

/* VM flags */
#define VM_READ (1 << 0)
#define VM_WRITE (1 << 1)
#define VM_EXEC (1 << 2)
#define VM_SHARED (1 << 3)
#define VM_ANON (1 << 4)

/* Flags for mmap_anonymous() to influence mapping behaviour */
#define VM_MAP_IMMEDIATE (1 << 16) /* allocate & map pages immediately */

#define PAGE_SHIFT 12 /* standard shift for 4KB pages */
#ifndef PAGE_SIZE
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif

#define PHYS_PFN(addr) ((addr) >> PAGE_SHIFT) /* convert address to PFN */

/* Forward declarations */
struct vm_area_struct;
struct mm_struct;

/* mm_struct represents an address space and holds linked list of VMAs */
typedef struct mm_struct {
	struct vm_area_struct *mmap; /* singly-linked sorted by address */
	u32 map_count;
} mm_struct;

/* VMA structure representing a contiguous virtual mapping */
typedef struct vm_area_struct {
	unsigned long vm_start; /* inclusive start address */
	unsigned long vm_end;   /* exclusive end address */
	unsigned long vm_pgoff; /* page offset for mapping */
	u32 vm_flags;	       /* VM_* flags */
	struct vm_area_struct *vm_next;
} vm_area_struct;

/* helpers */

/* Return number of pages spanned by the vma */
static inline unsigned long vma_pages(vm_area_struct *vma)
{
	return (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
}

/* mm management */
mm_struct *mm_create(void);
void mm_destroy(mm_struct *mm);

/* VMA allocation / free helpers */
vm_area_struct *vm_area_alloc(void);
void vm_area_free(vm_area_struct *vma);

/* Find VMA containing or next after addr */
vm_area_struct *find_vma(mm_struct *mm, unsigned long addr);
/* Find previous vma before addr; if prevp provided store prev */
vm_area_struct *find_vma_prev(mm_struct *mm, unsigned long addr,
			      vm_area_struct **prevp);

/* Insert/Remove VMAs (sorted). insert fails on overlap */
int insert_vm_struct(mm_struct *mm, vm_area_struct *vma);
void remove_vm_struct(mm_struct *mm, vm_area_struct *vma);

/* Split a VMA at addr (addr must be inside vma). Returns upper vma. */
vm_area_struct *split_vma_at(mm_struct *mm, vm_area_struct *vma,
			     unsigned long addr);

/*
 * mmap_anonymous:
 *  - mm: target address space
 *  - addr: page-aligned address where to map; if 0, function returns error
 *  - len: length in bytes (will be page-aligned)
 *  - flags: VM_* or VM_MAP_IMMEDIATE
 *
 * Returns KERNEL_OK on success, negative kernel_status_t on error. On success
 * if out_addr != NULL then *out_addr contains the mapping start.
 */
kernel_status_t mmap_anonymous(mm_struct *mm, unsigned long addr, size_t len,
			       u32 flags, unsigned long *out_addr);

/* munmap: unmaps and removes VMAs that overlap [addr, addr+len] */
kernel_status_t munmap_range(mm_struct *mm, unsigned long addr, size_t len);

/* Dump the mmap list for debugging */
void dump_mmap(mm_struct *mm);

#endif /* MM_VMA_H */