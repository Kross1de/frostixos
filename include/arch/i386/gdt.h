#ifndef ARCH_I386_GDT_H
#define ARCH_I386_GDT_H

#include <kernel/kernel.h>

#define GDT_ACCESS_PRESENT     0x80
#define GDT_ACCESS_RING0       0x00
#define GDT_ACCESS_RING1       0x20
#define GDT_ACCESS_RING2       0x40
#define GDT_ACCESS_RING3       0x60
#define GDT_ACCESS_SYSTEM      0x10
#define GDT_ACCESS_EXECUTABLE  0x08
#define GDT_ACCESS_DC          0x04
#define GDT_ACCESS_RW          0x02
#define GDT_ACCESS_ACCESSED    0x01

#define GDT_FLAG_GRANULARITY   0x80
#define GDT_FLAG_SIZE          0x40
#define GDT_FLAG_LONG          0x20
#define GDT_FLAG_AVAILABLE     0x10

#define GDT_KERNEL_CODE_ACCESS (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW)
#define GDT_KERNEL_DATA_ACCESS (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM | GDT_ACCESS_RW)
#define GDT_USER_CODE_ACCESS   (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW)
#define GDT_USER_DATA_ACCESS   (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM | GDT_ACCESS_RW)

#define GDT_STANDARD_FLAGS     (GDT_FLAG_GRANULARITY | GDT_FLAG_SIZE)

#define GDT_NULL_SELECTOR        0x00
#define GDT_KERNEL_CODE_SELECTOR 0x08
#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_USER_CODE_SELECTOR   0x18
#define GDT_USER_DATA_SELECTOR   0x20

#define GDT_MAX_ENTRIES 6

typedef struct {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  flags_limit_high;
    u8  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) gdt_ptr_t;

kernel_status_t gdt_init(void);
void gdt_set_gate(u8 index, u32 base, u32 limit, u8 access, u8 flags);
void gdt_load(void);

#endif // ARCH_I386_GDT_H
