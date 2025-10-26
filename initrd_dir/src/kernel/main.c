/*
 * Kernel main entry point.
 */
#include <arch/i386/acpi.h>
#include <arch/i386/cpuid.h>
#include <arch/i386/e820.h>
#include <arch/i386/gdt.h>
#include <arch/i386/idt.h>
#include <arch/i386/multiboot.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <drivers/ps2.h>
#include <drivers/serial.h>
#include <drivers/time.h>
#include <drivers/vbe.h>
#include <drivers/vga_text.h>
#include <drivers/initrd.h>
#include <kernel/kernel.h>
#include <lib/font.h>
#include <lib/terminal.h>
#include <misc/logger.h>
#include <misc/shell.h>
#include <mm/bitmap.h>
#include <mm/heap.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <printf.h>

#if !defined(__i386__)
#error "This kernel targets i386 architecture only."
#endif

u32 _multiboot_info_ptr = 0;
terminal_t g_terminal;

/* Kernel entry */
void kernel_main(u32 multiboot_magic, multiboot_info_t *multiboot_info)
{
	_multiboot_info_ptr = (u32)multiboot_info;
	kernel_status_t status;

	status = multiboot_init(multiboot_magic, multiboot_info);
	if (status != KERNEL_OK)
		kernel_panic("multiboot_init failed");

	/* Early drivers */
	vga_text_init();
	serial_init();

	/* CPU features */
	cpuid_init();

	/* Graphics and font */
	vbe_init();
	font_init();

	/* Terminal */
	terminal_init(&g_terminal);

	/* Descriptor tables */
	gdt_init();
	idt_init();

	/* Timer */
	pit_init(100);

	sti();
    
    //kernel_panic("test");

	/* Memory managers */
	pmm_init(multiboot_info);
	vmm_init();
    
    /* Memory map (E820) */
    status = e820_init();
    if (status != KERNEL_OK) {
        kernel_panic("e820_init failed");
    }
    e820_print_map();

	/* Discover and map initrd (if present) so higher-level code can consume it */
    status = initrd_init(multiboot_info);
    if (status != KERNEL_OK) {
        kernel_panic("initrd_init failed");
    }

    status = slab_init();
    if (status != KERNEL_OK) {
        kernel_panic("slab_init failed");
    }

	heap_init();

	/* ACPI, time and input */
	acpi_init();
	time_init();

	status = ps2_keyboard_init();
	if (status != KERNEL_OK)
		kernel_panic("PS/2 init failed");

	/* Start interactive shell */
	shell_start();

	/* Fallback message if shell returns */
	printf("Welcome to FrostixOS!\n");

	/* Probe CPU */
	cpuid_vendor_t vendor;
	cpuid_features_t features;
	cpuid_extended_t extended;

	cpuid_get_vendor(&vendor);
	printf("CPU Vendor: %s\n", vendor.vendor);

	cpuid_get_features(&features);
	printf("CPU Features - EAX: 0x%x, EBX: 0x%x, ECX: 0x%x, EDX: 0x%x\n",
	       features.eax, features.ebx, features.ecx, features.edx);

	cpuid_get_extended(&extended);
	printf("CPU Brand: %s\n", extended.brand_string);

	/* Idle */
	for (;;) {
		draw_status();
		asm volatile("hlt");
	}
}
