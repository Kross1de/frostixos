#ifndef KERNEL_KSYM_H
#define KERNEL_KSYM_H

#include <kernel/kernel.h>

struct ksym
{
    u32 addr;
    const char *name;
} __attribute__((packed));

extern struct ksym __start_ksym[] __attribute__((weak));
extern struct ksym __stop_ksym[] __attribute__((weak));

/* Export a symbol into .ksym. */
#define EXPORT_KSYM(sym) \
    static const struct ksym __ksym_##sym __attribute__((section(".ksym"), used)) = { (u32)(uintptr_t)(&sym), #sym }

/* Lookup API */
const char *ksym_lookup(u32 addr, u32 *offset);
size_t ksym_count(void);

#endif /* KERNEL_KSYM_H */