#include <arch/i386/gdt.h>
#include <arch/i386/idt.h>
#include <arch/i386/multiboot.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <drivers/serial.h>
#include <drivers/vbe.h>
#include <drivers/vga_text.h>
#include <kernel/kernel.h>
#include <lib/font.h>
#include <lib/terminal.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <printf.h>

#if !defined(__i386__)
#error "This kernel targets i386 architecture only."
#endif

u32 _multiboot_info_ptr = 0;
terminal_t g_terminal;

void kernel_main(u32 multiboot_magic, multiboot_info_t *multiboot_info) {
  _multiboot_info_ptr = (u32)multiboot_info;
  kernel_status_t status;

  status = multiboot_init(multiboot_magic, multiboot_info);
  if (status != KERNEL_OK) {
    kernel_panic("Failed to initialize multiboot information");
  }

  vga_text_init();
  serial_init();
  vbe_init();
  font_init();
  terminal_init(&g_terminal);
  gdt_init();
  idt_init();
  pit_init(10);
  sti();
  pmm_init(multiboot_info);
  printf("Welcome to FrostixOS!\n");
  //log(LOG_INFO, "Info log");
  //log(LOG_WARN, "Warn log");
  //log(LOG_ERR, " Err  log");
  //log(LOG_OKAY, "Okay log");
  u32 page = pmm_alloc_page();
  printf("Allocated page at 0x%x\n", page);
  pmm_free_page(page);
  serial_printf("number: %d\n", 3);
  screen_draw_demo();
  // kernel_panic("Test");
}
