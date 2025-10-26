/*
 * PIT timer initialization and handler
 */
#include <arch/i386/isr.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <drivers/time.h>
#include <lib/terminal.h>

extern u64 g_ticks;
extern const u32 pit_frequency;

static void pit_handler(registers_t *regs)
{
	UNUSED(regs);

	g_ticks++;

	if (g_ticks % pit_frequency == 0)
		time_update();

	/* Send EOI to PIC */
	outb(0x20, 0x20);
}

void pit_init(u32 frequency)
{
	isr_register_handler(32, pit_handler);

	u32 divisor = 1193180U / frequency;
	outb(0x43, 0x36);

	u8 low = (u8)(divisor & 0xFF);
	u8 high = (u8)((divisor >> 8) & 0xFF);
	outb(0x40, low);
	outb(0x40, high);

	/* Unmask IRQ0 (timer) */
	pic_unmask(0);
}