#ifndef ARCH_I386_IDT_H
#define ARCH_I386_IDT_H

#include "isr.h"
#include <kernel/kernel.h>

/*
 * Interrupt Descriptor Table (IDT) definitions for i386.
 * IDT entries map interrupt vectors to handler addresses and attributes.
 */
#define IDT_ENTRIES 256

typedef struct {
	u16 base_low;	/* low 16 bits of handler address */
	u16 sel;	/* kernel segment selector */
	u8 always0;
	u8 flags;	/* type and attributes */
	u16 base_high;	/* high 16 bits of handler address */
} __attribute__((packed)) idt_entry_t;

typedef struct {
	u16 limit;
	u32 base;
} __attribute__((packed)) idt_ptr_t;

/* IDT initialization and gate helper prototypes */
void idt_init(void);
void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags);
void idt_load(void);

#endif /* ARCH_I386_IDT_H */