#ifndef ARCH_I386_PIC_H
#define ARCH_I386_PIC_H

#include <kernel/kernel.h>

/*
 * Programmable Interrupt Controller (PIC) helpers.
 *
 * pic_remap() changes the IRQ vector offsets for master/slave PICs.
 * pic_mask_all() masks all IRQs.
 * pic_unmask() unmasks a single IRQ.
 * pic_send_eoi() signals end-of-interrupt for the given IRQ.
 */
void pic_remap(u8 offset1, u8 offset2);
void pic_mask_all(void);
void pic_unmask(u8 irq);   /* unmask single irq */
void pic_send_eoi(u8 irq); /* send end-of-interrupt to PIC */

#endif /* ARCH_I386_PIC_H */