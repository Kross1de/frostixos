#ifndef ARCH_I386_PIC_H
#define ARCH_I386_PIC_H

#include <kernel/kernel.h>

void pic_remap(u8 offset1, u8 offset2);
void pic_mask_all(void);
void pic_unmask(u8 irq); // unmask single irq

#endif // ARCH_I386_PIC_H
