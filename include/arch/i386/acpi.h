#ifndef ACPI_H
#define ACPI_H

#include <kernel/kernel.h>

typedef struct {
  char signature[8];
  u8 checksum;
  char oemid[6];
  u8 revision;
  u32 rsdt_addr;
} __attribute__((packed)) rsdp_t;

typedef struct {
  char signature[8];
  u8 checksum;
  char oemid[6];
  u8 revision;
  u32 rsdt_addr;
  u32 length;
  u64 xsdt_addr;
  u8 ext_checksum;
  u8 reserved[3];
} __attribute__((packed)) xsdp_t;

typedef struct {
  char sig[4];
  u32 length;
  u8 rev;
  u8 checksum;
  char oemid[6];
  char oem_table_id[8];
  u32 oem_rev;
  u32 creator_id;
  u32 creator_rev;
} __attribute__((packed)) sdt_header_t;

typedef struct {
  sdt_header_t header;
  u32 other_sdts[];
} __attribute__((packed)) rsdt_t;

typedef struct {
  sdt_header_t header;
  u64 other_sdts[];
} __attribute__((packed)) xsdt_t;

typedef struct {
  u8 address_space_id;
  u8 register_bit_width;
  u8 register_bit_offset;
  u8 access_size;
  u64 address;
} __attribute__((packed)) gas_t;

typedef struct {
  sdt_header_t header;
  u32 firmware_ctrl;
  u32 dsdt_addr;
  u8 reserved1;
  u8 preferred_pm_profile;
  u16 sci_int;
  u32 smi_cmd;
  u8 acpi_enable;
  u8 acpi_disable;
  u8 s4bios_req;
  u8 pstate_cnt;
  u32 pm1a_evt_blk;
  u32 pm1b_evt_blk;
  u32 pm1a_cnt_blk;
  u32 pm1b_cnt_blk;
  u32 pm2_cnt_blk;
  u32 pm_tmr_blk;
  u32 gpe0_blk;
  u32 gpe1_blk;
  u8 pm1_evt_len;
  u8 pm1_cnt_len;
  u8 pm2_cnt_len;
  u8 pm_tmr_len;
  u8 gpe0_blk_len;
  u8 gpe1_blk_len;
  u8 gpe1_base;
  u8 cst_cnt;
  u16 p_lvl2_lat;
  u16 p_lvl3_lat;
  u16 flush_size;
  u16 flush_stride;
  u8 duty_offset;
  u8 duty_width;
  u8 day_alrm;
  u8 mon_alrm;
  u8 century;
  u16 iapc_boot_arch;
  u8 reserved2;
  u32 flags;
  gas_t reset_reg;
  u8 reset_value;
  u16 arm_boot_arch;
  u8 fadt_minor_version;
  u64 x_firmware_ctrl;
  u64 x_dsdt_addr;
  gas_t x_pm1a_evt_blk;
  gas_t x_pm1b_evt_blk;
  gas_t x_pm1a_cnt_blk;
  gas_t x_pm1b_cnt_blk;
  gas_t x_pm2_cnt_blk;
  gas_t x_pm_tmr_blk;
  gas_t x_gpe0_blk;
  gas_t x_gpe1_blk;
  gas_t sleep_control_reg;
  gas_t sleep_status_reg;
  u64 hypervisor_vendor_id;
} __attribute__((packed)) fadt_t;

typedef struct {
  sdt_header_t header;
  u32 local_apic_addr;
  u32 flags;
  // ...
} __attribute__((packed)) madt_t;

void acpi_init(void);
sdt_header_t *acpi_get_table(const char *sig);

#endif
