#include <arch/i386/acpi.h>
#include <arch/i386/cpuid.h>
#include <arch/i386/gdt.h>
#include <arch/i386/idt.h>
#include <arch/i386/multiboot.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <drivers/serial.h>
#include <drivers/time.h>
#include <drivers/vbe.h>
#include <drivers/vga_text.h>
#include <kernel/kernel.h>
#include <lib/font.h>
#include <lib/terminal.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <printf.h>

#if !defined(__i386__)
#error "This kernel targets i386 architecture only."
#endif

u32 _multiboot_info_ptr = 0;
terminal_t g_terminal;

void kernel_main(u32 multiboot_magic, multiboot_info_t *multiboot_info) {
  _multiboot_info_ptr = (u32)multiboot_info;
  kernel_status_t status;
  cpuid_vendor_t vendor;
  cpuid_features_t features;
  cpuid_extended_t extended;

  status = multiboot_init(multiboot_magic, multiboot_info);
  if (status != KERNEL_OK) {
    kernel_panic("Failed to initialize multiboot information");
  }

  vga_text_init();
  serial_init();
  cpuid_init();
  vbe_init();
  font_init();
  terminal_init(&g_terminal);
  gdt_init();
  idt_init();
  acpi_init();
  pit_init(100);
  sti();
  pmm_init(multiboot_info);
  vmm_init();
  heap_init();
  time_init();

  printf("Welcome to FrostixOS!\n");

  cpuid_get_vendor(&vendor);
  printf("CPU Vendor: %s\n", vendor.vendor);
  cpuid_get_features(&features);
  printf("CPU Features - EAX: 0x%x, EBX: 0x%x, ECX: 0x%x, EDX: 0x%x\n",
         features.eax, features.ebx, features.ecx, features.edx);
  cpuid_get_extended(&extended);
  printf("CPU Brand: %s\n", extended.brand_string);

  void *test = kcalloc(10, 1024);
  if (test != NULL) {
    test = krealloc(test, 2048);
    kfree(test);
  }
  heap_get_total_size();
  heap_get_free_size();

  for (;;) {
    draw_status();
    asm volatile("hlt");
  }
}
