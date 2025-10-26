/*
 * CPUID helpers
 */
#include <arch/i386/cpuid.h>
#include <printf.h>
#include <string.h>

/* Detect CPUID support by toggling ID bit in EFLAGS */
bool cpuid_is_supported(void)
{
	u32 flags_before, flags_after;

	__asm__ volatile("pushf\n\t"
			 "pop %0\n\t"
			 : "=r"(flags_before));

	/* toggle ID bit (bit 21) */
	flags_after = flags_before ^ (1U << 21);

	__asm__ volatile("push %0\n\t"
			 "popf\n\t"
			 "pushf\n\t"
			 "pop %0\n\t"
			 : "=r"(flags_after)
			 : "r"(flags_after));

	return (flags_before != flags_after);
}

/* wrapper for cpuid instruction */
static void cpuid(u32 function, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	__asm__ volatile("cpuid"
			 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
			 : "a"(function)
			 : "memory");
}

kernel_status_t cpuid_init(void)
{
	return cpuid_is_supported() ? KERNEL_OK : KERNEL_ERROR;
}

kernel_status_t cpuid_get_vendor(cpuid_vendor_t *vendor)
{
	if (!vendor)
		return KERNEL_INVALID_PARAM;

	if (!cpuid_is_supported())
		return KERNEL_ERROR;

	u32 eax, ebx, ecx, edx;
	cpuid(CPUID_GET_VENDOR_ID, &eax, &ebx, &ecx, &edx);

	char *ptr = vendor->vendor;
	memcpy(ptr, &ebx, 4);
	memcpy(ptr + 4, &edx, 4);
	memcpy(ptr + 8, &ecx, 4);
	ptr[12] = '\0';

	return KERNEL_OK;
}

kernel_status_t cpuid_get_features(cpuid_features_t *features)
{
	if (!features)
		return KERNEL_INVALID_PARAM;

	if (!cpuid_is_supported())
		return KERNEL_ERROR;

	u32 eax, ebx, ecx, edx;
	cpuid(CPUID_GET_FEATURES, &eax, &ebx, &ecx, &edx);

	features->eax = eax;
	features->ebx = ebx;
	features->ecx = ecx;
	features->edx = edx;

	return KERNEL_OK;
}

kernel_status_t cpuid_get_extended(cpuid_extended_t *extended)
{
	if (!extended)
		return KERNEL_INVALID_PARAM;

	if (!cpuid_is_supported())
		return KERNEL_ERROR;

	u32 eax, ebx, ecx, edx;
	cpuid(CPUID_GET_EXTENDED_INFO, &eax, &ebx, &ecx, &edx);
	extended->max_extended_id = eax;

	if (extended->max_extended_id >= 0x80000004U) {
		char *ptr = extended->brand_string;
		for (u32 func = 0x80000002; func <= 0x80000004; func++) {
			cpuid(func, &eax, &ebx, &ecx, &edx);
			memcpy(ptr, &eax, 4);
			memcpy(ptr + 4, &ebx, 4);
			memcpy(ptr + 8, &ecx, 4);
			memcpy(ptr + 12, &edx, 4);
			ptr += 16;
		}
		extended->brand_string[47] = '\0';
	} else {
		extended->brand_string[0] = '\0';
	}

	return KERNEL_OK;
}