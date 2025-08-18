#ifndef ARCH_I386_IDT_H
#define ARCH_I386_IDT_H

#include <kernel/kernel.h>
#include "isr.h"

#define IDT_ENTRIES 256

typedef struct {
    u16 base_low;
    u16 sel;
    u8  always0;
    u8  flags;
    u16 base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) idt_ptr_t;

void idt_init(void);
void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags);
void idt_load(void);

#endif // ARCH_I386_IDT_H
