#include <arch/i386/acpi.h>
#include <misc/logger.h>
#include <string.h>

static uint8_t acpi_checksum(const void *table, uint32_t length) {
  uint8_t sum = 0;
  const uint8_t *ptr = (const uint8_t *)table;
  for (uint32_t i = 0; i < length; i++) {
    sum += ptr[i];
  }
  return sum;
}

rsdp_t *acpi_find_rsdp(void) {
  uint16_t ebda_seg = *((uint16_t *)0x40E);
  uint32_t ebda_addr = (uint32_t)ebda_seg << 4;
  for (uint32_t ptr = ebda_addr; ptr < ebda_addr + 1024; ptr += 16) {
    if (memcmp((void *)ptr, "RSD PTR ", 8) == 0) {
      if (acpi_checksum((void *)ptr, 20) == 0) {
        return (rsdp_t *)ptr;
      }
    }
  }
  for (uint32_t ptr = 0xE0000; ptr < 0x100000; ptr += 16) {
    if (memcmp((void *)ptr, "RSD PTR ", 8) == 0) {
      if (acpi_checksum((void *)ptr, 20) == 0) {
        return (rsdp_t *)ptr;
      }
    }
  }
  return NULL; // RSDP not found
}

void acpi_init(void) {
  rsdp_t *rsdp = acpi_find_rsdp();
  if (rsdp == NULL) {
    log(LOG_ERR, "ACPI: RSDP not found.");
    return;
  }
  log(LOG_INFO, "ACPI: RSDP found at 0x%x, Revision: %d, OEM: %s",
      (uint32_t)rsdp, rsdp->revision, rsdp->oemid);
  if (rsdp->revision >= 2) {
    rsdt_t *rsdt = (rsdt_t *)rsdp->rsdt_addr;
    if (acpi_checksum(rsdt, rsdt->header.length) != 0) {
      log(LOG_ERR, "ACPI: RSDT checksum invalid.");
      return;
    }
    uint32_t num_tables =
        (rsdt->header.length - sizeof(sdt_header_t)) / sizeof(uint32_t);
    log(LOG_INFO, "ACPI: %d tables available in RSDT.", num_tables);
    for (uint32_t i = 0; i < num_tables; i++) {
      sdt_header_t *header = (sdt_header_t *)rsdt->other_sdts[i];
      if (acpi_checksum(header, header->length) == 0) {
        log(LOG_INFO, "ACPI Table: %.4s (Rev %d, OEM: %.6s)", header->sig,
            header->rev, header->oemid);
      }
    }
    xsdp_t *xsdp = (xsdp_t *)rsdp;
    if (acpi_checksum(xsdp, xsdp->length) != 0) {
      log(LOG_ERR, "ACPI: Extended checksum invalid.");
      return;
    }
    log(LOG_INFO, "ACPI: XSDT at 0x%llx", xsdp->xsdt_addr);
  } else {
    log(LOG_INFO, "ACPI: RSDT at 0x%x", rsdp->rsdt_addr);
  }
}
