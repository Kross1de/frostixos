#include <arch/i386/multiboot.h>
#include <kernel/kernel.h>

static multiboot_info_t* g_multiboot_info = NULL;
static u32 g_multiboot_magic = 0;

kernel_status_t multiboot_init(u32 magic, multiboot_info_t* mbi) {
    if (!mbi) {
        return KERNEL_INVALID_PARAM;
    }
    
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        return KERNEL_ERROR;
    }
    
    g_multiboot_magic = magic;
    g_multiboot_info = mbi;
    
    return KERNEL_OK;
}

u32 multiboot_get_memory_size(void) {
    if (!g_multiboot_info) {
        return 0;
    }
    
    if (!(g_multiboot_info->flags & 0x00000001)) {
        return 0;
    }
    
    return g_multiboot_info->mem_lower + g_multiboot_info->mem_upper;
}

multiboot_info_t* multiboot_get_info(void) {
    return g_multiboot_info;
}
