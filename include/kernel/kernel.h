#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>
#include <stdint.h>

#define FROSTIX_VERSION_MAJOR 0
#define FROSTIX_VERSION_MINOR 1
#define FROSTIX_VERSION_PATCH 0
#define FROSTIX_VERSION_STRING "0.1.0"

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        s8;
typedef signed short       s16;
typedef signed short       i16;
typedef signed int         s32;
typedef signed long long   s64;

typedef enum {
    false = 0,
    true = 1
} bool;

typedef enum {
    KERNEL_OK = 0,
    KERNEL_ERROR = -1,
    KERNEL_INVALID_PARAM = -2,
    KERNEL_OUT_OF_MEMORY = -3,
    KERNEL_NOT_IMPLEMENTED = -4
} kernel_status_t;

#define ALIGN_UP(addr, align)   (((addr) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))
#define PAGE_SIZE 4096
#define PAGE_ALIGN(addr) ALIGN_UP(addr, PAGE_SIZE)

#define UNUSED(x) ((void)(x))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define sti() __asm__ volatile ("sti")
#define cli() __asm__ volatile ("cli")

static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void kernel_panic(const char* message) __attribute__((noreturn));

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
void* memmove(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);

#endif // KERNEL_H
