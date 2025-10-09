#ifndef KERNEL_H
#define KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FROSTIX_VERSION_MAJOR 0
#define FROSTIX_VERSION_MINOR 1
#define FROSTIX_VERSION_PATCH 0
#define FROSTIX_VERSION_STRING "0.1.0"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed short i16;
typedef signed int s32;
typedef signed long long s64;

typedef enum {
  KERNEL_OK = 0,
  KERNEL_ERROR = -1,
  KERNEL_INVALID_PARAM = -2,
  KERNEL_OUT_OF_MEMORY = -3,
  KERNEL_NOT_IMPLEMENTED = -4,
  KERNEL_ALREADY_MAPPED = -5
} kernel_status_t;

#define ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))
#define PAGE_SIZE 4096
#define PAGE_ALIGN(addr) ALIGN_UP(addr, PAGE_SIZE)

#define UNUSED(x) ((void)(x))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define sti() __asm__ volatile("sti")
#define cli() __asm__ volatile("cli")

static inline void outb(u16 port, u8 val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
  u8 ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outw(u16 port, u16 val) {
  __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port) {
  u16 ret;
  __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outl(u16 port, u32 val) {
  __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port) {
  u32 ret;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

void kernel_panic(const char *message) __attribute__((noreturn));

#endif // KERNEL_H
