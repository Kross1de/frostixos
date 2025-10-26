#ifndef DRIVERS_INITRD_H
#define DRIVERS_INITRD_H

#include <arch/i386/multiboot.h>
#include <kernel/kernel.h>
#include <stdint.h>
#include <stddef.h>

/* Initialize initrd subsystem: find and register first multiboot module */
kernel_status_t initrd_init(multiboot_info_t *mb_info);

/* Get pointer to initrd data (virtual address). NULL if none. */
void *initrd_get_data(void);

/* Get initrd size in bytes. 0 if none. */
uint32_t initrd_get_size(void);

/* Helper: list files in the registered initrd (if any). Returns
 * KERNEL_OK on success, error otherwise. */
kernel_status_t initrd_list(void);

/* Helper: find a file by path inside the initrd; returns pointer to file
 * data (start of file) and sets out_size. Returns NULL on not found. */
const void *initrd_find(const char *path, size_t *out_size);

#endif /* DRIVERS_INITRD_H */