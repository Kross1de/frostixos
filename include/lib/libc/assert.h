#ifndef ASSERT_H
#define ASSERT_H

#include <kernel/kernel.h>

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) \
	((expr) ? (void)0 : kernel_assert(#expr, __FILE__, __LINE__))
#endif

void kernel_assert(const char *expr, const char *file, int line)
	__attribute__((noreturn));

#endif