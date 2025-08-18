#include <kernel/kernel.h>
#include <drivers/vbe.h>
#include <drivers/font.h>
#include <drivers/screen.h>
#include <stddef.h>

static void draw_panic(const char* message) {
    vbe_clear_screen(VBE_COLOR_RED);
    screen_draw_rect(20, 20, vbe_get_device()->width - 40, vbe_get_device()->height - 40, VBE_COLOR_DARK_GRAY, VBE_COLOR_WHITE);
    screen_draw_string_centered(60, "!!! KERNEL PANIC !!!", VBE_COLOR_WHITE, VBE_COLOR_RED);
    screen_draw_string(40, 120, message, VBE_COLOR_YELLOW, VBE_COLOR_DARK_GRAY);
    screen_draw_string(40, 160, "System halted. Please restart your computer.", VBE_COLOR_WHITE, VBE_COLOR_DARK_GRAY);
}

void kernel_panic(const char* message) {
    __asm__ volatile ("cli");

    if (vbe_is_available() && vbe_get_device() && vbe_get_device()->initialized) {
        draw_panic(message ? message : "Unknown panic");
    } else { // use VGA text
        if (message) {
            volatile u16* vga_buffer = (volatile u16*)0xB8000;
            u8 panic_color = 0x4F;

            for (size_t i = 0; i < 160; i++) {
                vga_buffer[i] = ' ' | ((u16)panic_color << 8);
            }

            const char* panic_prefix = "KERNEL PANIC: ";
            size_t prefix_len = 14;
            size_t pos = 0;

            for (size_t i = 0; i < prefix_len && pos < 80; i++, pos++) {
                vga_buffer[pos] = (u16)panic_prefix[i] | ((u16)panic_color << 8);
            }

            for (const char* p = message; *p && pos < 80; p++, pos++) {
                vga_buffer[pos] = (u16)*p | ((u16)panic_color << 8);
            }

            while (pos < 80) {
                vga_buffer[pos++] = ' ' | ((u16)panic_color << 8);
            }

            const char* halt_msg = "System halted. Please restart your computer.";
            pos = 80;

            for (const char* p = halt_msg; *p && pos < 160; p++, pos++) {
                vga_buffer[pos] = (u16)*p | ((u16)panic_color << 8);
            }

            while (pos < 160) {
                vga_buffer[pos++] = ' ' | ((u16)panic_color << 8);
            }
        }
    }

    while (1) {
        __asm__ volatile ("hlt");
    }
}
