#include <kernel/stacktrace.h>
#include <kernel/ksym.h>
#include <kernel/kernel.h>
#include <printf.h>
#include <stdint.h>
#include <stddef.h>

struct stackframe {
    struct stackframe *ebp;
    u32 eip;
};

void stack_trace_to_buffer(char *buf, size_t bufsize)
{
    if (!buf || bufsize == 0) return;

    size_t off = 0;
    off += snprintf(buf + off, (off < bufsize) ? bufsize - off : 0, "--- STACK TRACE ---\n");

    struct stackframe *frame;
    __asm__ volatile ("movl %%ebp, %0" : "=r" (frame));

    const unsigned int max_frames = 32;
    for (unsigned int i = 0; frame && i < max_frames; ++i) {
        u32 ret = frame->eip;
        if (ret == 0) break;

        u32 sym_off = 0;
        const char *name = ksym_lookup(ret, &sym_off);

        off += snprintf(buf + off, (off < bufsize) ? bufsize - off : 0,
                        "  [%02u] 0x%08x: %s+0x%x\n", i, ret, name, sym_off);

        /* Advance to next frame with naive safety checks. */
        struct stackframe *next = frame->ebp;
        if (!next) break;

        /* stack grows down; next should be a higher pointer value (less negative) */
        if (next <= frame) break;

        /* Avoid following utterly insane pointers */
        if ((uintptr_t)next > (uintptr_t)0xF0000000UL) break;

        frame = next;
    }

    off += snprintf(buf + off, (off < bufsize) ? bufsize - off : 0, "--------------------\n");

    /* Ensure NUL termination */
    if (off >= bufsize) buf[bufsize - 1] = '\0';
}