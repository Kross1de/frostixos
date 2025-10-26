#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <kernel/kernel.h>
#include <stddef.h>

/*
 * Kernel heap API (kmalloc/kfree style).
 *
 * heap_init() initializes kernel heap structures. kmalloc/kfree provide
 * dynamic memory allocation inside the kernel. kcalloc/krealloc are helper
 * variants and helper queries return heap statistics.
 */
kernel_status_t heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t new_size);
void kfree(void *ptr);
size_t heap_get_total_size(void);
size_t heap_get_free_size(void);

#endif /* MM_HEAP_H */