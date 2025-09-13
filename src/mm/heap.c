#include <kernel/kernel.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/heap.h>
#include <mm/vmm.h>

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

static kernel_status_t heap_expand(size_t additional_size) {
  additional_size = PAGE_ALIGN(additional_size + sizeof(struct heap_block));
  u32 num_pages = additional_size / PAGE_SIZE;

  log(LOG_INFO, "Expanding heap by %u bytes (%u pages) at 0x%x",
      additional_size, num_pages, heap_current_end);

  u32 phys_addr = pmm_alloc_pages(num_pages);
  if (phys_addr == 0) {
    log(LOG_ERR, "Failed to allocate %u physical pages", num_pages);
    return KERNEL_OUT_OF_MEMORY;
  }

  kernel_status_t status = vmm_map_pages(heap_current_end, phys_addr, num_pages,
                                         PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
  if (status != KERNEL_OK) {
    log(LOG_ERR, "Failed to map %u pages at 0x%x, status: %d", num_pages,
        heap_current_end, status);
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

  log(LOG_INFO, "Created new block at 0x%x, size: %u, free: %d", (u32)new_block,
      new_block->size, new_block->free);

  if (heap_head == NULL) {
    heap_head = new_block;
    log(LOG_INFO, "Set heap_head to new block at 0x%x", (u32)heap_head);
  } else {
    struct heap_block *last = heap_head;
    while (last->next != NULL) {
      last = last->next;
    }
    last->next = new_block;
    new_block->prev = last;
    log(LOG_INFO, "Linked new block at 0x%x to last block at 0x%x",
        (u32)new_block, (u32)last);
  }

  heap_current_end += additional_size;
  log(LOG_INFO, "Updated heap_current_end to 0x%x", heap_current_end);
  return KERNEL_OK;
}

kernel_status_t heap_init(void) {
  heap_head = NULL;
  heap_current_end = HEAP_START;
  log(LOG_INFO, "Initializing heap at 0x%x with size %u", HEAP_START,
      INITIAL_HEAP_SIZE);
  kernel_status_t status = heap_expand(INITIAL_HEAP_SIZE);
  if (status != KERNEL_OK) {
    log(LOG_ERR, "Heap initialization failed, status: %d", status);
    return status;
  }
  log(LOG_OKAY, "Heap initialized successfully");
  return KERNEL_OK;
}

void *kmalloc(size_t size) {
  if (size == 0) {
    log(LOG_WARN, "kmalloc called with size 0, returning NULL");
    return NULL;
  }

  size = HEAP_ALIGN(size);
  log(LOG_INFO, "kmalloc requested for %u bytes (aligned)", size);

  struct heap_block *current = heap_head;
  while (current != NULL) {
    if (current->free && current->size >= size &&
        current->magic == HEAP_MAGIC) {
      log(LOG_INFO, "Found free block at 0x%x, size: %u", (u32)current,
          current->size);
      if (current->size > size + sizeof(struct heap_block)) {
        struct heap_block *new_block =
            (struct heap_block *)((u8 *)current + sizeof(struct heap_block) +
                                  size);
        new_block->next = current->next;
        new_block->prev = current;
        new_block->size = current->size - size - sizeof(struct heap_block);
        new_block->free = true;
        new_block->magic = HEAP_MAGIC;
        current->next = new_block;
        if (new_block->next) {
          new_block->next->prev = new_block;
        }
        current->size = size;
        log(LOG_INFO, "Split block at 0x%x, new block at 0x%x, size: %u",
            (u32)current, (u32)new_block, new_block->size);
      }

      current->free = false;
      log(LOG_INFO, "Allocated block at 0x%x, returning 0x%x", (u32)current,
          (u32)((u8 *)current + sizeof(struct heap_block)));
      return (void *)((u8 *)current + sizeof(struct heap_block));
    }
    current = current->next;
  }

  log(LOG_INFO, "No suitable block found, expanding heap for %u bytes",
      size + sizeof(struct heap_block));
  size_t expand_size = size + sizeof(struct heap_block);
  kernel_status_t status = heap_expand(expand_size);
  if (status != KERNEL_OK) {
    log(LOG_ERR, "Heap expansion failed, status: %d", status);
    return NULL;
  }

  return kmalloc(size);
}

void kfree(void *ptr) {
  if (ptr == NULL) {
    log(LOG_WARN, "kfree called with NULL pointer");
    return;
  }

  struct heap_block *block =
      (struct heap_block *)((u8 *)ptr - sizeof(struct heap_block));
  log(LOG_INFO, "kfree called for block at 0x%x (ptr: 0x%x)", (u32)block,
      (u32)ptr);

  if (block->magic != HEAP_MAGIC) {
    log(LOG_ERR, "Invalid heap block at 0x%x", (u32)block);
    return;
  }

  if (!block->free) {
    block->free = true;
    log(LOG_INFO, "Marked block at 0x%x as free, size: %u", (u32)block,
        block->size);

    if (block->next != NULL && block->next->free &&
        block->next->magic == HEAP_MAGIC) {
      block->size += sizeof(struct heap_block) + block->next->size;
      block->next = block->next->next;
      if (block->next) {
        block->next->prev = block;
      }
      log(LOG_INFO, "Merged block at 0x%x with next, new size: %u", (u32)block,
          block->size);
    }

    if (block->prev != NULL && block->prev->free &&
        block->prev->magic == HEAP_MAGIC) {
      block->prev->size += sizeof(struct heap_block) + block->size;
      block->prev->next = block->next;
      if (block->next) {
        block->next->prev = block->prev;
      }
      log(LOG_INFO, "Merged block at 0x%x with previous at 0x%x, new size: %u",
          (u32)block, (u32)block->prev, block->prev->size);
    }
  } else {
    log(LOG_WARN, "Attempted to free already free block at 0x%x", (u32)block);
  }
}