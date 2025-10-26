#ifndef ARCH_I386_CPUID_H
#define ARCH_I386_CPUID_H

#include <kernel/kernel.h>

/*
 * CPUID helper wrappers and types.
 * These provide convenience structures for vendor, features and extended info.
 */

#define CPUID_GET_VENDOR_ID 0x0
#define CPUID_GET_FEATURES 0x1
#define CPUID_GET_EXTENDED_INFO 0x80000000

typedef struct {
	char vendor[13];	/* vendor string (12 bytes + NUL) */
} cpuid_vendor_t;

typedef struct {
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
} cpuid_features_t;

typedef struct {
	u32 max_extended_id;
	char brand_string[48];
} cpuid_extended_t;

/* CPUID subsystem API */
kernel_status_t cpuid_init(void);
kernel_status_t cpuid_get_vendor(cpuid_vendor_t *vendor);
kernel_status_t cpuid_get_features(cpuid_features_t *features);
kernel_status_t cpuid_get_extended(cpuid_extended_t *extended);
bool cpuid_is_supported(void);

#endif /* ARCH_I386_CPUID_H */