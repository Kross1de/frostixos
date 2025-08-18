#include <arch/i386/idt.h>

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idtp;

extern void idt_flush(u32);

void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags) {
    idt[num].base_low  = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

void idt_init(void) {
    idtp.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idtp.base  = (u32)&idt;

    memset(&idt, 0, sizeof(idt_entry_t) * IDT_ENTRIES);

    isr_init();

    idt_load();

    //sti();
}

void idt_load(void) {
    idt_flush((u32)&idtp);
}
