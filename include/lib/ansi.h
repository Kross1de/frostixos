#ifndef LIB_ANSI_H
#define LIB_ANSI_H

#include <drivers/vbe.h>
#include <lib/ansi_types.h>
#include <lib/font.h>
#include <lib/terminal.h>
#include <stdlib.h>

/*
 * ANSI/VT100 parser API.
 *
 * ansi_init() initializes the parser context.
 * ansi_process_char() feeds a single character into the parser which may
 * update the terminal state (cursor movement, color changes, etc.).
 */
void ansi_init(ansi_context_t *ctx);
void ansi_process_char(ansi_context_t *ctx, terminal_t *term, char c);

#endif /* LIB_ANSI_H */