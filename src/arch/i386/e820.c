#include <arch/i386/e820.h>
#include <arch/i386/multiboot.h>
#include <kernel/kernel.h>
#include <misc/logger.h>
#include <printf.h>

/* Global E820 map */
e820_map_t g_e820_map = {0};

static void e820_reset(void)
{
    g_e820_map.entry_count = 0;
}

static void e820_add_entry(u64 base, u64 length, u32 type, u32 ext)
{
    if (g_e820_map.entry_count >= E820_MAX_ENTRIES)
        return;
    e820_entry_t *e = &g_e820_map.entries[g_e820_map.entry_count++];
    e->base_addr = base;
    e->length = length;
    e->type = type;
    e->extended = ext;
}

kernel_status_t e820_detect_memory(void)
{
    multiboot_info_t *mbi = multiboot_get_info();
    if (!mbi)
        return KERNEL_INVALID_PARAM;
    
    if (!(mbi->flags & MULTIBOOT_INFO_MEM_MAP))
        return KERNEL_ERROR;
    
    e820_reset();
    
    multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
    while ((u32)mmap < mbi->mmap_addr + mbi->mmap_length) {
        u32 type = mmap->type;
        if (type == 0)
            type = E820_TYPE_RESERVED;
        e820_add_entry(mmap->addr, mmap->len, type, 0);
        mmap = (multiboot_memory_map_t *) ((u32)mmap + mmap->size + sizeof(mmap->size));
    }
    
    return KERNEL_OK;
}

kernel_status_t e820_init(void)
{
    return e820_detect_memory();
}

static void print_entry(const e820_entry_t *e, u32 idx)
{
    u64 end = e->base_addr + e->length;
    printf("E820[%02u]: 0x%016llx - 0x%016llx (%10llu KB) %s\n",
            idx,
            (unsigned long long)e->base_addr,
            (unsigned long long)(end ? end - 1 : 0),
            (unsigned long long)(e->length / 1024ULL),
            e820_type_to_string(e->type));
}

void e820_print_map(void)
{
    log(LOG_INFO, "E820 memory map (%u entries):", g_e820_map.entry_count);
    for (u32 i = 0; i < g_e820_map.entry_count; i++)
        print_entry(&g_e820_map.entries[i], i);
}

u64 e820_get_total_usable_memory(void)
{
    u64 total = 0;
    for (u32 i = 0; i < g_e820_map.entry_count; i++) {
        const e820_entry_t *e = &g_e820_map.entries[i];
        if (e->type == E820_TYPE_USABLE)
            total += e->length;
    }
    return total;
}

u64 e820_get_highest_address(void)
{
    u64 max = 0;
    for (u32 i = 0; i < g_e820_map.entry_count; i++) {
        const e820_entry_t *e = &g_e820_map.entries[i];
        if (e->base_addr + e->length > max)
            max = e->base_addr + e->length;
    }
    return max;
}

e820_entry_t *e820_find_region(u64 address)
{
    for (u32 i = 0; i < g_e820_map.entry_count; i++) {
        e820_entry_t *e = &g_e820_map.entries[i];
        if (address >= e->base_addr && address < (e->base_addr + e->length))
            return e;
    }
    return NULL;
}

bool e820_is_usable_region(u64 base, u64 length)
{
    if (length == 0)
        return false;
    for (u32 i = 0; i < g_e820_map.entry_count; i++) {
        const e820_entry_t *e = &g_e820_map.entries[i];
        if (e->type != E820_TYPE_USABLE)
            continue;
        if (base >= e->base_addr && (base + length) <= (e->base_addr + e->length))
            return true;
    }
    return false;
}

const char *e820_type_to_string(u32 type)
{
    switch (type) {
    case E820_TYPE_USABLE:
        return "USABLE";
    case E820_TYPE_RESERVED:
        return "RESERVED";
    case E820_TYPE_ACPI_RECLAIM:
        return "ACPI_RECLAIM";
    case E820_TYPE_ACPI_NVS:
        return "ACPI_NVS";
    case E820_TYPE_BAD_MEMORY:
        return "BAD_MEMORY";
    case E820_TYPE_PMEM:
        return "PMEM";
    case E820_TYPE_PRAM:
        return "PRAM";
    default:
        return "UNKNOWN";
    }
}