#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/heap.h>
#include <mm/slab.h>
#include <mm/vma.h>
#include <mm/vmm.h>
#include <printf.h>
#include <string.h>

#define VMA_MAGIC 0xBEEFBEEF

/* unmapped_area: find a free virtual region of 'len' bytes.
 * Strategy: scan VMAs in mm->mmap for gaps starting from a sane base.
 * Returns 0 on failure or the aligned start virtual address on success.
 */
static unsigned long unmapped_area(mm_struct *mm, unsigned long len)
{
	if (!mm || len == 0)
		return 0;

	/* Search from a conservative userland base upwards.
	 * Keep within a large canonical user range for this simple kernel.
	 */
	const unsigned long SEARCH_START = 0x20000000UL; /* 512MB */
	const unsigned long SEARCH_END = 0xF0000000UL;   /* avoid kernel/top area */

	unsigned long need = ALIGN_UP(len, PAGE_SIZE);
	vm_area_struct *v = mm->mmap;
	unsigned long addr = SEARCH_START;

	/* If no mappings exist, return SEARCH_START if it fits */
	if (!v) {
		if (addr + need < SEARCH_END)
			return addr;
		return 0;
	}

	/* Walk the VMAs and find a gap large enough */
	while (v) {
		/* If the VMA starts after our candidate and the gap fits -> use it */
		if (v->vm_start > addr) {
			unsigned long gap_end = v->vm_start;
			if (gap_end >= addr && gap_end - addr >= need) {
				return addr;
			}
		}
		/* Move candidate to just after this VMA */
		if (v->vm_end > addr)
			addr = ALIGN_UP(v->vm_end, PAGE_SIZE);

		/* If candidate exceeds search end, fail */
		if (addr + need >= SEARCH_END)
			return 0;

		v = v->vm_next;
	}

	/* No intervening VMA; end of list available */
	if (addr + need < SEARCH_END)
		return addr;

	return 0;
}

/* mm management */
mm_struct *mm_create(void)
{
	mm_struct *mm = (mm_struct *)kmalloc(sizeof(mm_struct));
	if (!mm)
		return NULL;
	mm->mmap = NULL;
	mm->map_count = 0;
	return mm;
}

void mm_destroy(mm_struct *mm)
{
	if (!mm)
		return;

	/* remove and free all VMAs */
	vm_area_struct *v = mm->mmap;
	while (v) {
		vm_area_struct *next = v->vm_next;
		/* best-effort unmap pages */
		unsigned long pages = vma_pages(v);
		for (unsigned long i = 0; i < pages; ++i)
			vmm_unmap_page(v->vm_start + i * PAGE_SIZE);
		vm_area_free(v);
		v = next;
	}
	kfree(mm);
}

/* vm area alloc/free */
struct vm_area_struct *vm_area_alloc(void)
{
	vm_area_struct *v = (vm_area_struct *)kmalloc(sizeof(vm_area_struct));
	if (!v)
		return NULL;
	memset(v, 0, sizeof(*v));
	return v;
}

void vm_area_free(struct vm_area_struct *vma)
{
	if (!vma)
		return;
	kfree(vma);
}

/* returns first vma with vm_end > addr (i.e. may contain addr or be the next)
 */
struct vm_area_struct *find_vma(mm_struct *mm, unsigned long addr)
{
	if (!mm)
		return NULL;
	vm_area_struct *v = mm->mmap;
	while (v) {
		if (v->vm_end > addr)
			return v;
		v = v->vm_next;
	}
	return NULL;
}

/* find previous vma before addr; if prevp provided store prev */
struct vm_area_struct *find_vma_prev(mm_struct *mm, unsigned long addr,
				     struct vm_area_struct **prevp)
{
	if (!mm) {
		if (prevp)
			*prevp = NULL;
		return NULL;
	}
	vm_area_struct *prev = NULL;
	vm_area_struct *v = mm->mmap;
	while (v) {
		if (v->vm_end > addr) {
			if (prevp)
				*prevp = prev;
			return v;
		}
		prev = v;
		v = v->vm_next;
	}
	if (prevp)
		*prevp = prev;
	return NULL;
}

/* insert vma in sorted list; fail (-1) if overlaps */
int insert_vm_struct(mm_struct *mm, struct vm_area_struct *vma)
{
	if (!mm || !vma)
		return -1;
	if (vma->vm_start >= vma->vm_end)
		return -1;

	/* empty list */
	if (!mm->mmap) {
		mm->mmap = vma;
		vma->vm_next = NULL;
		mm->map_count++;
		return 0;
	}

	vm_area_struct *cur = mm->mmap;
	vm_area_struct *prev = NULL;
	while (cur && cur->vm_start < vma->vm_start) {
		prev = cur;
		cur = cur->vm_next;
	}

	if (prev && prev->vm_end > vma->vm_start)
		return -1;
	if (cur && vma->vm_end > cur->vm_start)
		return -1;

	if (!prev) {
		vma->vm_next = mm->mmap;
		mm->mmap = vma;
	} else {
		vma->vm_next = prev->vm_next;
		prev->vm_next = vma;
	}
	mm->map_count++;
	return 0;
}

/* remove vma from list (does not free vma) */
void remove_vm_struct(mm_struct *mm, struct vm_area_struct *vma)
{
	if (!mm || !vma)
		return;
	vm_area_struct *prev = NULL;
	vm_area_struct *cur = mm->mmap;
	while (cur) {
		if (cur == vma) {
			if (!prev)
				mm->mmap = cur->vm_next;
			else
				prev->vm_next = cur->vm_next;
			mm->map_count--;
			cur->vm_next = NULL;
			return;
		}
		prev = cur;
		cur = cur->vm_next;
	}
}

/* split vma at addr; returns newly created upper vma pointer or NULL on error
 */
struct vm_area_struct *split_vma_at(mm_struct *mm, struct vm_area_struct *vma,
				    unsigned long addr)
{
	if (!mm || !vma)
		return NULL;
	if (addr <= vma->vm_start || addr >= vma->vm_end)
		return NULL;

	vm_area_struct *upper = vm_area_alloc();
	if (!upper)
		return NULL;

	upper->vm_flags = vma->vm_flags;
	upper->vm_pgoff = vma->vm_pgoff + PHYS_PFN(addr - vma->vm_start);
	upper->vm_start = addr;
	upper->vm_end = vma->vm_end;

	vma->vm_end = addr;

	/* insert upper after vma */
	upper->vm_next = vma->vm_next;
	vma->vm_next = upper;
	mm->map_count++;
	return upper;
}

/*
 * mmap_anonymous: simple anonymous mapping
 * - Align addr and len to page boundaries.
 * - If addr == 0 -> find a free region via unmapped_area()
 * - Insert VMA
 * - If flags include VM_MAP_IMMEDIATE -> allocate pages and map them
 */
kernel_status_t mmap_anonymous(mm_struct *mm, unsigned long addr, size_t len,
			       u32 flags, unsigned long *out_addr)
{
	if (!mm || len == 0)
		return KERNEL_INVALID_PARAM;

	unsigned long start = ALIGN_DOWN(addr, PAGE_SIZE);
	unsigned long end = ALIGN_UP(addr + len, PAGE_SIZE);

	if (start == 0) {
		/* allow caller to request an automatic mapping address */
		start = 0;
	}

	if (end <= start && addr != 0)
		return KERNEL_INVALID_PARAM;

	/* if caller requests addr==0 (automatic placement), find a region */
	if (addr == 0) {
		unsigned long candidate = unmapped_area(mm, len);
		if (candidate == 0)
			return KERNEL_OUT_OF_MEMORY;
		start = candidate;
		end = candidate + ALIGN_UP(len, PAGE_SIZE);
	}

	if (start == 0)
		return KERNEL_INVALID_PARAM;

	if (end <= start)
		return KERNEL_INVALID_PARAM;

	vm_area_struct *vma = vm_area_alloc();
	if (!vma)
		return KERNEL_OUT_OF_MEMORY;

	vma->vm_start = start;
	vma->vm_end = end;
	vma->vm_pgoff = start >> PAGE_SHIFT;
	vma->vm_flags = VM_ANON
			| (flags & (VM_READ | VM_WRITE | VM_EXEC | VM_SHARED));

	if (insert_vm_struct(mm, vma) != 0) {
		vm_area_free(vma);
		return KERNEL_ALREADY_MAPPED;
	}

	/* Optionally map pages immediately */
	if (flags & VM_MAP_IMMEDIATE) {
		unsigned long pages = (end - start) >> PAGE_SHIFT;
		for (unsigned long i = 0; i < pages; ++i) {
			unsigned long va = start + i * PAGE_SIZE;
			u32 phys = pmm_alloc_page();
			if (!phys) {
				/* rollback: unmap previous pages and remove vma */
				for (unsigned long j = 0; j < i; ++j) {
					vmm_unmap_page(start + j * PAGE_SIZE);
				}
				remove_vm_struct(mm, vma);
				vm_area_free(vma);
				return KERNEL_OUT_OF_MEMORY;
			}
			kernel_status_t st = vmm_map_page(
				va, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
			if (st != KERNEL_OK) {
				pmm_free_page(phys);
				for (unsigned long j = 0; j < i; ++j) {
					vmm_unmap_page(start + j * PAGE_SIZE);
				}
				remove_vm_struct(mm, vma);
				vm_area_free(vma);
				return st;
			}
		}
	}

	if (out_addr)
		*out_addr = start;
	return KERNEL_OK;
}

/*
 * munmap_range: unmaps and removes VMAs overlapping [addr, addr+len)
 * - Splits partial coverings
 * - Unmaps pages from vmm
 */
kernel_status_t munmap_range(mm_struct *mm, unsigned long addr, size_t len)
{
	if (!mm || len == 0)
		return KERNEL_INVALID_PARAM;

	unsigned long start = ALIGN_DOWN(addr, PAGE_SIZE);
	unsigned long end = ALIGN_UP(addr + len, PAGE_SIZE);

	vm_area_struct *v = find_vma(mm, start);
	vm_area_struct *prev = NULL;

	/* If first vma starts before range and overlaps, split it */
	if (v && v->vm_start < start && v->vm_end > start) {
		vm_area_struct *upper = split_vma_at(mm, v, start);
		if (!upper)
			return KERNEL_OUT_OF_MEMORY;
		prev = v;
		v = upper;
	}

	while (v && v->vm_start < end) {
		/* If v extends past end, split tail and remove the lower part */
		if (v->vm_end > end) {
			vm_area_struct *upper = split_vma_at(mm, v, end);
			if (!upper)
				return KERNEL_OUT_OF_MEMORY;
			/* unmap pages of v (now lower) */
			unsigned long pages = vma_pages(v);
			for (unsigned long i = 0; i < pages; ++i)
				vmm_unmap_page(v->vm_start + i * PAGE_SIZE);

			/* remove v from list */
			if (prev)
				prev->vm_next = upper;
			else
				mm->mmap = upper;
			mm->map_count--;
			vm_area_free(v);
			return KERNEL_OK;
		}

		/* v fully inside [start,end) */
		vm_area_struct *next = v->vm_next;
		unsigned long pages = vma_pages(v);
		for (unsigned long i = 0; i < pages; ++i)
			vmm_unmap_page(v->vm_start + i * PAGE_SIZE);

		if (prev)
			prev->vm_next = next;
		else
			mm->mmap = next;
		mm->map_count--;
		vm_area_free(v);

		v = next;
	}

	return KERNEL_OK;
}

void dump_mmap(mm_struct *mm)
{
	if (!mm) {
		printf("No mm_struct provided.\n");
		return;
	}
	printf("MM has %d mappings:\n", mm->map_count);
	vm_area_struct *v = mm->mmap;
	int index = 0;
	while (v) {
		printf("VMA #%d: start=0x%lx, end=0x%lx, size=%lu pages, "
		       "flags=0x%x, "
		       "pgoff=0x%lx\n",
		       index++, v->vm_start, v->vm_end, vma_pages(v),
		       v->vm_flags, v->vm_pgoff);
		v = v->vm_next;
	}
	if (mm->map_count == 0) {
		printf("No VMAs currently mapped.\n");
	}
}