#ifndef ARCH_I386_E820_H
#define ARCH_I386_E820_H

#include <kernel/kernel.h>

/*
 * BIOS E820 memory detection interface for i386.
 */

#define E820_MAX_ENTRIES 128

/* E820 memory region types */
#define E820_TYPE_USABLE         1  /* Available RAM */
#define E820_TYPE_RESERVED       2  /* Reserved - unusable */
#define E820_TYPE_ACPI_RECLAIM   3  /* ACPI reclaimable memory */
#define E820_TYPE_ACPI_NVS       4  /* ACPI NVS memory */
#define E820_TYPE_BAD_MEMORY     5  /* Bad memory */

/* Extended E820 types (newer systems) */
#define E820_TYPE_PMEM           7  /* Persistent memory */
#define E820_TYPE_PRAM           12 /* Persistent RAM */

/* E820 memory map entry */
typedef struct {
    u64 base_addr;      /* Base addess of memory region */
    u64 length;         /* Length of memory region in bytes */
    u32 type;           /* Memory type (E820_TYPE_*) */
    u32 extended;       /* Extended attributes (ACPI 3.0+) */
} __attribute__((packed)) e820_entry_t;

/* E820 memory map structure */
typedef struct {
    u32 entry_count;                        /* Number of valid entries */
    e820_entry_t entries[E820_MAX_ENTRIES]; /* Memory map entries */
} e820_map_t;

/* Global E820 memory map */
extern e820_map_t g_e820_map;

/* E820 API functions */
kernel_status_t e820_init(void);
kernel_status_t e820_detect_memory(void);
void e820_print_map(void);
u64 e820_get_total_usable_memory(void);
u64 e820_get_highest_address(void);
e820_entry_t *e820_find_region(u64 address);
bool e820_is_usable_region(u64 base, u64 length);

/* Helper functions for memory type names */
const char *e820_type_to_string(u32 type);

#endif  /* ARCH_I386_E820_H */