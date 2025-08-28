#ifndef ARCH_I386_ISR_H
#define ARCH_I386_ISR_H

#include <kernel/kernel.h>

typedef struct {
    u32 ds;

    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;

    u32 int_no;
    u32 err_code;

    u32 eip;
    u32 cs;
    u32 eflags;
    u32 useresp;
    u32 ss;
} registers_t;

typedef void (*isr_handler_t)(registers_t* regs);

void isr_init(void);
void isr_register_handler(u8 n, isr_handler_t handler);

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr32(void);  // IRQ0
extern void isr33(void);  // IRQ1
extern void isr34(void);  // IRQ2
extern void isr35(void);  // IRQ3
extern void isr36(void);  // IRQ4
extern void isr37(void);  // IRQ5
extern void isr38(void);  // IRQ6
extern void isr39(void);  // IRQ7
extern void isr40(void);  // IRQ8
extern void isr41(void);  // IRQ9
extern void isr42(void);  // IRQ10
extern void isr43(void);  // IRQ11
extern void isr44(void);  // IRQ12
extern void isr45(void);  // IRQ13
extern void isr46(void);  // IRQ14
extern void isr47(void);  // IRQ15

#endif // ARCH_I386_ISR_H
