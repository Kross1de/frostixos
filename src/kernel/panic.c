/*
 * Kernel panic: show message via VBE if available, otherwise VGA text.
 */
#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/font.h>
#include <stddef.h>
#include <printf.h>
#include <string.h>
#include <kernel/stacktrace.h>

static void draw_panic(const char *message)
{
    /* Ensure font is ready even if kernel panicked before init */
    font_init();

    vbe_device_t *device = vbe_get_device();

    vbe_clear_screen(VBE_COLOR_DARK_BLUE);

    vbe_draw_string_centered(50, "!!! KERNEL PANIC !!!", VBE_COLOR_RED, VBE_COLOR_DARK_BLUE);
    vbe_draw_string_centered(80, message ? message : "Unknown panic", VBE_COLOR_YELLOW, VBE_COLOR_DARK_BLUE);
    vbe_draw_string_centered(110, "System halted. Please restart your computer.", VBE_COLOR_WHITE, VBE_COLOR_DARK_BLUE);

    char buf[1024];
    stack_trace_to_buffer(buf, sizeof(buf));

    const char *p = buf;
    int x_pos = 20;
    int y_pos = 150;
    char line[256];

    while (*p && y_pos < (int)device->height - 50) {
        size_t i = 0;
        while (*p && *p != '\n' && i + 1 < sizeof(line)) {
            line[i++] = *p++;
        }
        line[i] = '\0';
        if (*p == '\n') p++;

        if (i > 0) {
            vbe_draw_string(x_pos, y_pos, line, VBE_COLOR_WHITE, VBE_COLOR_DARK_BLUE);
            y_pos += 16;
        }
    }

    char version_str[64];
    snprintf(version_str, sizeof(version_str), "FrostixOS Version: %s", FROSTIX_VERSION_STRING);
    vbe_draw_string_centered(device->height - 30, version_str, VBE_COLOR_LIGHT_GRAY, VBE_COLOR_DARK_BLUE);
}

void kernel_panic(const char *message)
{
    __asm__ volatile("cli");

    if (vbe_is_available() && vbe_get_device() && vbe_get_device()->initialized) {
        draw_panic(message ? message : "Unknown panic");
    } else {
        /* VGA fallback */
        if (message) {
            volatile u16 *vga_buffer = (volatile u16 *)0xB8000;
            u8 panic_color = 0x4F;

            for (size_t i = 0; i < 160; i++)
                vga_buffer[i] = ' ' | ((u16)panic_color << 8);

            const char *prefix = "KERNEL PANIC: ";
            size_t pos = 0;
            for (const char *p = prefix; *p && pos < 80; p++, pos++)
                vga_buffer[pos] = (u16)*p | ((u16)panic_color << 8);

            for (const char *p = message; *p && pos < 80; p++, pos++)
                vga_buffer[pos] = (u16)*p | ((u16)panic_color << 8);

            while (pos < 80)
                vga_buffer[pos++] = ' ' | ((u16)panic_color << 8);

            const char *halt_msg = "System halted. Please restart your computer.";
            pos = 80;
            for (const char *p = halt_msg; *p && pos < 160; p++, pos++)
                vga_buffer[pos] = (u16)*p | ((u16)panic_color << 8);

            while (pos < 160)
                vga_buffer[pos++] = ' ' | ((u16)panic_color << 8);
        }
    }

    for (;;) __asm__ volatile("hlt");
}
