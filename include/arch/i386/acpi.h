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

void acpi_init(void);

#endif
