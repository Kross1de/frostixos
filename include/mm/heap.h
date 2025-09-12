#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <kernel/kernel.h>
#include <stddef.h>

kernel_status_t heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif