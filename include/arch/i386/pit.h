#ifndef ARCH_I386_PIT_H
#define ARCH_I386_PIT_H

#include <kernel/kernel.h>

/*
 * Programmable Interval Timer (PIT) initialization.
 *
 * pit_init() programs the PIT to the requested frequency and hooks
 * the timer interrupt for system ticks.
 */
void pit_init(u32 frequency);

#endif /* ARCH_I386_PIT_H */