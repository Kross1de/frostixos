#ifndef ARCH_I386_PIC_H
#define ARCH_I386_PIC_H

#include <kernel/kernel.h>

void pic_remap(u8 offset1, u8 offset2);
void pic_mask_all(void);
void pic_unmask(u8 irq);   // unmask single irq
void pic_send_eoi(u8 irq); // send end-of-interrupt to PIC

#endif // ARCH_I386_PIC_H
