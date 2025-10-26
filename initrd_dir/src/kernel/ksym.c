#include <kernel/ksym.h>
#include <kernel/kernel.h>
#include <stddef.h>

/* Legacy symbol table layout (from symbols.c) */
struct legacy_symbol {
    u32 addr;
    const char *name;
};

/* weak so missing symbol won't cause link failure */
extern struct ksym __start_ksym[] __attribute__((weak));
extern struct ksym __stop_ksym[]  __attribute__((weak));

extern struct legacy_symbol symbols[] __attribute__((weak));
extern size_t num_symbols __attribute__((weak));

/* Linear search: fine for small tables. */
const char *ksym_lookup(u32 addr, u32 *offset)
{
    struct ksym *start = __start_ksym;
    struct ksym *stop  = __stop_ksym;
    
    if (start && stop && start != stop) {
        u32 best_addr = 0;
        const char *best_name = "<unknown>";

        for (struct ksym *s = start; s < stop; ++s) {
            if (s->addr <= addr && s->addr > best_addr) {
                best_addr = s->addr;
                best_name = s->name;
            }
        }

        if (offset) *offset = addr - best_addr;
        return best_name;
    }

    /* Fallback to legacy symbols[] table if present */
    if (symbols && num_symbols > 0) {
        u32 best_addr = 0;
        const char *best_name = "<unknown>";

        for (size_t i = 0; i < num_symbols; ++i) {
            if (symbols[i].addr <= addr && symbols[i].addr > best_addr) {
                best_addr = symbols[i].addr;
                best_name = symbols[i].name;
            }
        }

        if (offset) *offset = addr - best_addr;
        return (best_addr == 0) ? "<no-symbols>" : best_name;
    }

    /* Nothing available */
    if (offset) *offset = addr;
    return "<no-symbols>";
}

size_t ksym_count(void)
{
    struct ksym *start = __start_ksym;
    struct ksym *stop  = __stop_ksym;

    if (start && stop && start != stop) {
        return (size_t)(stop - start);
    }

    if (symbols && num_symbols > 0) {
        return num_symbols;
    }

    return 0;
}