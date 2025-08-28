#include <kernel/kernel.h>
#include <drivers/vga_text.h>
#include <drivers/vbe.h>
#include <drivers/font.h>
#include <arch/i386/multiboot.h>
#include <arch/i386/gdt.h>
#include <arch/i386/idt.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <drivers/terminal.h>

#if defined(__linux__)
#error "Cross-compiler required! Use i686-elf-gcc."
#endif

#if !defined(__i386__)
#error "This kernel targets i386 architecture only."
#endif

u32 _multiboot_info_ptr = 0;
terminal_t g_terminal;

void kernel_main(u32 multiboot_magic, multiboot_info_t* multiboot_info) {
    _multiboot_info_ptr = (u32)multiboot_info;
    kernel_status_t status;

    status = multiboot_init(multiboot_magic, multiboot_info);
    if (status != KERNEL_OK) {
        kernel_panic("Failed to initialize multiboot information");
    }

    vga_text_init();
    vbe_init();
    font_init();
    terminal_init(&g_terminal);
    gdt_init();
    idt_init();
    pic_remap(0x20, 0x28);
    pic_mask_all();
    pit_init(10);
    pic_unmask(0);
    sti();
    terminal_print(&g_terminal, "Hello world\n");
}
