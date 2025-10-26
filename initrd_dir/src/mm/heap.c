#include <kernel/kernel.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <string.h>

struct heap_block {
	struct heap_block *next;
	struct heap_block *prev;
	size_t size;
	bool free;
	u32 magic;
};

#define HEAP_MAGIC 0xDEADBEEF
#define HEAP_START 0xD0000000
#define INITIAL_HEAP_SIZE (4 * 1024 * 1024)
#define HEAP_ALIGN(size) ALIGN_UP(size, 4)

static struct heap_block *heap_head = NULL;
static u32 heap_current_end = HEAP_START;

/* Expand the heap area by at least the given size in bytes */
static kernel_status_t heap_expand(size_t additional_size)
{
	additional_size
		= PAGE_ALIGN(additional_size + sizeof(struct heap_block));
	u32 num_pages = additional_size / PAGE_SIZE;
	log(LOG_INFO, "Expanding heap by %u bytes (%u pages) at 0x%x",
	    additional_size, num_pages, heap_current_end);
	u32 phys_addr = pmm_alloc_pages(num_pages);
	if (phys_addr == 0) {
		log(LOG_ERR, "Failed to allocate %u physical pages", num_pages);
		return KERNEL_OUT_OF_MEMORY;
	}
	kernel_status_t status
		= vmm_map_pages(heap_current_end, phys_addr, num_pages,
				PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
	if (status != KERNEL_OK) {
		log(LOG_ERR, "Failed to map %u pages at 0x%x, status: %d",
		    num_pages, heap_current_end, status);
		pmm_free_pages(phys_addr, num_pages);
		return status;
	}
	struct heap_block *new_block = (struct heap_block *)heap_current_end;
	memset(new_block, 0, additional_size);
	new_block->size = additional_size - sizeof(struct heap_block);
	new_block->free = true;
	new_block->next = NULL;
	new_block->prev = NULL;
	new_block->magic = HEAP_MAGIC;
	log(LOG_INFO, "Created new block at 0x%x, size: %u", (u32)new_block,
	    new_block->size);
	if (heap_head == NULL) {
		heap_head = new_block;
	} else {
		struct heap_block *last = heap_head;
		while (last->next != NULL)
			last = last->next;
		last->next = new_block;
		new_block->prev = last;
	}
	heap_current_end += additional_size;
	return KERNEL_OK;
}

/* Try to merge adjacent free blocks */
static void heap_defragment(void)
{
	struct heap_block *current = heap_head;
	while (current != NULL && current->next != NULL) {
		if (current->free && current->next->free
		    && current->magic == HEAP_MAGIC
		    && current->next->magic == HEAP_MAGIC) {
			current->size += sizeof(struct heap_block)
					 + current->next->size;
			current->next = current->next->next;
			if (current->next)
				current->next->prev = current;
		} else {
			current = current->next;
		}
	}
}

kernel_status_t heap_init(void)
{
	heap_head = NULL;
	heap_current_end = HEAP_START;
	return heap_expand(INITIAL_HEAP_SIZE);
}

void *kmalloc(size_t size)
{
	if (size == 0) {
		log(LOG_WARN, "kmalloc called with size 0, returning NULL");
		return NULL;
	}

	struct heap_block *current = heap_head;
	while (current != NULL) {
		if (current->free && current->size >= size
		    && current->magic == HEAP_MAGIC) {
			if (current->size > size + sizeof(struct heap_block)) {
				struct heap_block *new_block
					= (struct heap_block
						   *)((u8 *)current
						      + sizeof(
							      struct heap_block)
						      + size);
				new_block->next = current->next;
				new_block->prev = current;
				new_block->size = current->size - size
						  - sizeof(struct heap_block);
				new_block->free = true;
				new_block->magic = HEAP_MAGIC;
				current->next = new_block;
				if (new_block->next) {
					new_block->next->prev = new_block;
				}
				current->size = size;
			}

			current->free = false;
			return (void *)((u8 *)current
					+ sizeof(struct heap_block));
		}
		current = current->next;
	}

	heap_defragment();

	current = heap_head;
	while (current != NULL) {
		if (current->free && current->size >= size
		    && current->magic == HEAP_MAGIC) {
			if (current->size > size + sizeof(struct heap_block)) {
				struct heap_block *new_block
					= (struct heap_block
						   *)((u8 *)current
						      + sizeof(
							      struct heap_block)
						      + size);
				new_block->next = current->next;
				new_block->prev = current;
				new_block->size = current->size - size
						  - sizeof(struct heap_block);
				new_block->free = true;
				new_block->magic = HEAP_MAGIC;
				current->next = new_block;
				if (new_block->next) {
					new_block->next->prev = new_block;
				}
				current->size = size;
			}

			current->free = false;
			return (void *)((u8 *)current
					+ sizeof(struct heap_block));
		}
		current = current->next;
	}

	kernel_status_t status = heap_expand(size + sizeof(struct heap_block));
	if (status != KERNEL_OK) {
		log(LOG_ERR, "Heap expansion failed, status: %d", status);
		return NULL;
	}

	return kmalloc(size);
}

void *kcalloc(size_t num, size_t size)
{
	size_t total_size = num * size;
	if (total_size == 0) {
		return NULL;
	}

	void *ptr = kmalloc(total_size);
	if (ptr != NULL) {
		memset(ptr, 0, total_size);
	}
	return ptr;
}

void *krealloc(void *ptr, size_t new_size)
{
	if (ptr == NULL) {
		return kmalloc(new_size);
	}

	if (new_size == 0) {
		kfree(ptr);
		return NULL;
	}

	struct heap_block *block
		= (struct heap_block *)((u8 *)ptr - sizeof(struct heap_block));
	if (block->magic != HEAP_MAGIC || block->free) {
		log(LOG_ERR, "krealloc invalid block at 0x%x", (u32)block);
		return NULL;
	}

	void *new_ptr = kmalloc(new_size);
	if (new_ptr == NULL) {
		return NULL;
	}

	size_t copy_size = MIN(block->size, new_size);
	memcpy(new_ptr, ptr, copy_size);
	kfree(ptr);

	return new_ptr;
}

size_t heap_get_total_size(void)
{
	size_t total = 0;
	struct heap_block *current = heap_head;
	while (current != NULL) {
		total += current->size + sizeof(struct heap_block);
		current = current->next;
	}
	log(LOG_INFO, "Total heap size: %u bytes", total);
	return total;
}

size_t heap_get_free_size(void)
{
	size_t free_size = 0;
	struct heap_block *current = heap_head;
	while (current != NULL) {
		if (current->free) {
			free_size += current->size;
		}
		current = current->next;
	}
	log(LOG_INFO, "Free heap size: %u bytes", free_size);
	return free_size;
}

void kfree(void *ptr)
{
	if (ptr == NULL) {
		return;
	}

	struct heap_block *block
		= (struct heap_block *)((u8 *)ptr - sizeof(struct heap_block));
	if (block->magic != HEAP_MAGIC) {
		log(LOG_ERR, "Invalid heap block at 0x%x", (u32)block);
		return;
	}

	if (!block->free) {
		block->free = true;

		if (block->next != NULL && block->next->free
		    && block->next->magic == HEAP_MAGIC) {
			block->size += sizeof(struct heap_block)
				       + block->next->size;
			block->next = block->next->next;
			if (block->next) {
				block->next->prev = block;
			}
		}

		if (block->prev != NULL && block->prev->free
		    && block->prev->magic == HEAP_MAGIC) {
			block->prev->size
				+= sizeof(struct heap_block) + block->size;
			block->prev->next = block->next;
			if (block->next) {
				block->next->prev = block->prev;
			}
		}
	}
}