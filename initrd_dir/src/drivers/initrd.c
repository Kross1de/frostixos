#include <arch/i386/multiboot.h>
#include <misc/logger.h>
#include <drivers/initrd.h>
#include <mm/vmm.h>
#include <printf.h>
#include <string.h>
#include <fs/tar.h>

static void *g_initrd_data = NULL;
static uint32_t g_initrd_size = 0;

kernel_status_t initrd_init(multiboot_info_t *mb_info)
{
    if (!mb_info)
        return KERNEL_INVALID_PARAM;

    if (!(mb_info->flags & MULTIBOOT_INFO_MODS)) {
        log(LOG_INFO, "INITRD: no modules present");
        return KERNEL_OK;
    }

    /* If mods_count is present, ensure we have at least one module. */
    /* NOTE: multiboot_info_t in tree should expose mods_count; if not,
       this check is harmless but redundant. */
    if (mb_info->mods_count == 0) {
        log(LOG_INFO, "INITRD: mods_count == 0");
        return KERNEL_OK;
    }

    multiboot_module_t *mod = (multiboot_module_t *)mb_info->mods_addr;
    if (!mod) {
        log(LOG_WARN, "INITRD: modules pointer NULL");
        return KERNEL_ERROR;
    }

    /* Use first module as initrd for now */
    uint32_t phys_start = mod->mod_start;
    uint32_t phys_end = mod->mod_end;
    uint32_t size = phys_end - phys_start;

    if (size == 0) {
        log(LOG_WARN, "INITRD: first module has zero size");
        return KERNEL_ERROR;
    }

    /* Align mapping to page boundaries */
    const uint32_t PAGE = 4096;
    uint32_t aligned_start = phys_start & ~(PAGE - 1);
    uint32_t aligned_end = (phys_end + PAGE - 1) & ~(PAGE - 1);
    uint32_t map_size = aligned_end - aligned_start;

    /* Ensure mapping exists for the physical range - identity mapping style:
     * the kernel uses identity mapping for low memory in many places.
     * vmm_map_if_not_mapped will map pages if necessary.
     */
    kernel_status_t st = vmm_map_if_not_mapped(aligned_start, map_size);
    if (st != KERNEL_OK) {
        log(LOG_ERR, "INITRD: failed to map initrd phys 0x%x size %u",
            aligned_start, map_size);
        return st;
    }

    /* Assume identity mapping (common in many kernels): virtual address is same
     * as physical. If your vmm maps to a different virtual base, adjust here. */
    g_initrd_data = (void *)(uintptr_t)phys_start;
    g_initrd_size = size;
    log(LOG_OKAY, "INITRD: registered at phys=0x%x size=%u", phys_start, size);

    return KERNEL_OK;
}

void *initrd_get_data(void)
{
    return g_initrd_data;
}

uint32_t initrd_get_size(void)
{
    return g_initrd_size;
}

kernel_status_t initrd_list(void)
{
    if (!g_initrd_data || g_initrd_size == 0) {
        log(LOG_INFO, "INITRD: no initrd present");
        return KERNEL_ERROR;
    }
    tar_list(g_initrd_data, g_initrd_size);
    return KERNEL_OK;
}

const void *initrd_find(const char *path, size_t *out_size)
{
    if (!g_initrd_data || g_initrd_size == 0)
        return NULL;
    return tar_find(g_initrd_data, g_initrd_size, path, out_size);
}
