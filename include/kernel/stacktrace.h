#ifndef KERNEL_STACKTRACE_H
#define KERNEL_STACKTRACE_H

#include <stddef.h>

/* Fill `buf` (size `bufsize`) with a stack trace. */
void stack_trace_to_buffer(char *buf, size_t bufsize);

#endif /* KERNEL_STACKTRACE_H */