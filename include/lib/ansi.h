#ifndef LIB_ANSI_H
#define LIB_ANSI_H

#include <drivers/vbe.h>
#include <lib/ansi_types.h>
#include <lib/terminal.h>
#include <lib/font.h>
#include <stdlib.h>

void ansi_init(ansi_context_t *ctx);
void ansi_process_char(ansi_context_t *ctx, terminal_t *term, char c);

#endif