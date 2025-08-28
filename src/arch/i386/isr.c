#include <arch/i386/isr.h>
#include <arch/i386/idt.h>
#include <drivers/screen.h>
#include <kernel/kernel.h>

isr_handler_t interrupt_handlers[256];

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

void isr_init(void) {
    idt_set_gate(0, (u32)isr0, 0x08, 0x8E);
    idt_set_gate(1, (u32)isr1, 0x08, 0x8E);
    idt_set_gate(2, (u32)isr2, 0x08, 0x8E);
    idt_set_gate(3, (u32)isr3, 0x08, 0x8E);
    idt_set_gate(4, (u32)isr4, 0x08, 0x8E);
    idt_set_gate(5, (u32)isr5, 0x08, 0x8E);
    idt_set_gate(6, (u32)isr6, 0x08, 0x8E);
    idt_set_gate(7, (u32)isr7, 0x08, 0x8E);
    idt_set_gate(8, (u32)isr8, 0x08, 0x8E);
    idt_set_gate(9, (u32)isr9, 0x08, 0x8E);
    idt_set_gate(10, (u32)isr10, 0x08, 0x8E);
    idt_set_gate(11, (u32)isr11, 0x08, 0x8E);
    idt_set_gate(12, (u32)isr12, 0x08, 0x8E);
    idt_set_gate(13, (u32)isr13, 0x08, 0x8E);
    idt_set_gate(14, (u32)isr14, 0x08, 0x8E);
    idt_set_gate(15, (u32)isr15, 0x08, 0x8E);
    idt_set_gate(16, (u32)isr16, 0x08, 0x8E);
    idt_set_gate(17, (u32)isr17, 0x08, 0x8E);
    idt_set_gate(18, (u32)isr18, 0x08, 0x8E);
    idt_set_gate(19, (u32)isr19, 0x08, 0x8E);
    idt_set_gate(20, (u32)isr20, 0x08, 0x8E);
    idt_set_gate(21, (u32)isr21, 0x08, 0x8E);
    idt_set_gate(22, (u32)isr22, 0x08, 0x8E);
    idt_set_gate(23, (u32)isr23, 0x08, 0x8E);
    idt_set_gate(24, (u32)isr24, 0x08, 0x8E);
    idt_set_gate(25, (u32)isr25, 0x08, 0x8E);
    idt_set_gate(26, (u32)isr26, 0x08, 0x8E);
    idt_set_gate(27, (u32)isr27, 0x08, 0x8E);
    idt_set_gate(28, (u32)isr28, 0x08, 0x8E);
    idt_set_gate(29, (u32)isr29, 0x08, 0x8E);
    idt_set_gate(30, (u32)isr30, 0x08, 0x8E);
    idt_set_gate(31, (u32)isr31, 0x08, 0x8E);
    idt_set_gate(32, (u32)isr32, 0x08, 0x8E);
    idt_set_gate(33, (u32)isr33, 0x08, 0x8E);
    idt_set_gate(34, (u32)isr34, 0x08, 0x8E);
    idt_set_gate(35, (u32)isr35, 0x08, 0x8E);
    idt_set_gate(36, (u32)isr36, 0x08, 0x8E);
    idt_set_gate(37, (u32)isr37, 0x08, 0x8E);
    idt_set_gate(38, (u32)isr38, 0x08, 0x8E);
    idt_set_gate(39, (u32)isr39, 0x08, 0x8E);
    idt_set_gate(40, (u32)isr40, 0x08, 0x8E);
    idt_set_gate(41, (u32)isr41, 0x08, 0x8E);
    idt_set_gate(42, (u32)isr42, 0x08, 0x8E);
    idt_set_gate(43, (u32)isr43, 0x08, 0x8E);
    idt_set_gate(44, (u32)isr44, 0x08, 0x8E);
    idt_set_gate(45, (u32)isr45, 0x08, 0x8E);
    idt_set_gate(46, (u32)isr46, 0x08, 0x8E);
    idt_set_gate(47, (u32)isr47, 0x08, 0x8E);

    for (int i = 0; i < 256; i++) {
        interrupt_handlers[i] = 0;
    }
}

void isr_register_handler(u8 n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

void isr_handler(registers_t* regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else if (regs->int_no < 32) {
        kernel_panic(exception_messages[regs->int_no]);
    }
}
