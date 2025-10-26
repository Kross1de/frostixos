#include <kernel/kernel.h>
#include <stddef.h>

struct symbol { u32 addr; const char *name; };
struct symbol symbols[] = {
    /* legacy table empty */
};
size_t num_symbols = 0;