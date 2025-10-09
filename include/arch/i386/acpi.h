#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

typedef struct {
  char signature[8];
  uint8_t checksum;
  char oemid[6];
  uint8_t revision;
  uint32_t rsdt_addr;
} __attribute__((packed)) rsdp_t;

typedef struct {
  char signature[8];
  uint8_t checksum;
  char oemid[6];
  uint8_t revision;
  uint32_t rsdt_addr;
  uint32_t length;
  uint64_t xsdt_addr;
  uint8_t ext_checksum;
  uint8_t reserved[3];
} __attribute__((packed)) xsdp_t;

typedef struct {
  char sig[4];
  uint32_t length;
  uint8_t rev;
  uint8_t checksum;
  char oemid[6];
  char oem_table_id[8];
  uint32_t oem_rev;
  uint32_t creator_id;
  uint32_t creator_rev;
} __attribute__((packed)) sdt_header_t;

typedef struct {
  sdt_header_t header;
  uint32_t other_sdts[];
} __attribute__((packed)) rsdt_t;

typedef struct {
  sdt_header_t header;
  uint64_t other_sdts[];
} __attribute__((packed)) xsdt_t;

typedef struct {
  uint8_t address_space_id;
  uint8_t register_bit_width;
  uint8_t register_bit_offset;
  uint8_t access_size;
  uint64_t address;
} __attribute__((packed)) gas_t;

typedef struct {
  sdt_header_t header;
  uint32_t firmware_ctrl;
  uint32_t dsdt_addr;
  uint8_t reserved1;
  uint8_t preferred_pm_profile;
  uint16_t sci_int;
  uint32_t smi_cmd;
  uint8_t acpi_enable;
  uint8_t acpi_disable;
  uint8_t s4bios_req;
  uint8_t pstate_cnt;
  uint32_t pm1a_evt_blk;
  uint32_t pm1b_evt_blk;
  uint32_t pm1a_cnt_blk;
  uint32_t pm1b_cnt_blk;
  uint32_t pm2_cnt_blk;
  uint32_t pm_tmr_blk;
  uint32_t gpe0_blk;
  uint32_t gpe1_blk;
  uint8_t pm1_evt_len;
  uint8_t pm1_cnt_len;
  uint8_t pm2_cnt_len;
  uint8_t pm_tmr_len;
  uint8_t gpe0_blk_len;
  uint8_t gpe1_blk_len;
  uint8_t gpe1_base;
  uint8_t cst_cnt;
  uint16_t p_lvl2_lat;
  uint16_t p_lvl3_lat;
  uint16_t flush_size;
  uint16_t flush_stride;
  uint8_t duty_offset;
  uint8_t duty_width;
  uint8_t day_alrm;
  uint8_t mon_alrm;
  uint8_t century;
  uint16_t iapc_boot_arch;
  uint8_t reserved2;
  uint32_t flags;
  gas_t reset_reg;
  uint8_t reset_value;
  uint16_t arm_boot_arch;
  uint8_t fadt_minor_version;
  uint64_t x_firmware_ctrl;
  uint64_t x_dsdt_addr;
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
  uint64_t hypervisor_vendor_id;
} __attribute__((packed)) fadt_t;

typedef struct {
  sdt_header_t header;
  uint32_t local_apic_addr;
  uint32_t flags;
  // ...
} __attribute__((packed)) madt_t;

void acpi_init(void);
sdt_header_t *acpi_get_table(const char *sig);

#endif
