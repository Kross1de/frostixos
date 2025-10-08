#include <arch/i386/gdt.h>
#include <kernel/kernel.h>
#include <string.h>

static gdt_entry_t gdt_entries[GDT_MAX_ENTRIES];
static gdt_ptr_t gdt_ptr;

extern void gdt_flush(u32 gdt_ptr_addr);

kernel_status_t gdt_init(void) {
  gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_MAX_ENTRIES) - 1;
  gdt_ptr.base = (u32)&gdt_entries;

  memset(&gdt_entries, 0, sizeof(gdt_entries));

  gdt_set_gate(0, 0, 0, 0, 0);
  gdt_set_gate(1, 0, 0xFFFFFFFF, GDT_KERNEL_CODE_ACCESS, GDT_STANDARD_FLAGS);
  gdt_set_gate(2, 0, 0xFFFFFFFF, GDT_KERNEL_DATA_ACCESS, GDT_STANDARD_FLAGS);
  gdt_set_gate(3, 0, 0xFFFFFFFF, GDT_USER_CODE_ACCESS, GDT_STANDARD_FLAGS);
  gdt_set_gate(4, 0, 0xFFFFFFFF, GDT_USER_DATA_ACCESS, GDT_STANDARD_FLAGS);

  gdt_load();

  return KERNEL_OK;
}

void gdt_set_gate(u8 index, u32 base, u32 limit, u8 access, u8 flags) {
  if (index >= GDT_MAX_ENTRIES) {
    return;
  }

  gdt_entry_t *entry = &gdt_entries[index];

  entry->base_low = base & 0xFFFF;
  entry->base_middle = (base >> 16) & 0xFF;
  entry->base_high = (base >> 24) & 0xFF;
  entry->limit_low = limit & 0xFFFF;
  entry->flags_limit_high = (limit >> 16) & 0x0F;
  entry->flags_limit_high |= flags & 0xF0;
  entry->access = access;
}

void gdt_load(void) { gdt_flush((u32)&gdt_ptr); }
