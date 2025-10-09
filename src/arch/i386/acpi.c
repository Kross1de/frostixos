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

sdt_header_t *acpi_get_table(const char *sig) {
  rsdp_t *rsdp = acpi_find_rsdp();
  if (rsdp == NULL)
    return NULL;

  void *sdt_ptr = NULL;
  uint32_t entry_size, num_entries;

  if (rsdp->revision >= 2) {
    xsdp_t *xsdp = (xsdp_t *)rsdp;
    if (acpi_checksum(xsdp, xsdp->length) != 0)
      return NULL;
    xsdt_t *xsdt = (xsdt_t *)(uintptr_t)xsdp->xsdt_addr;
    if (acpi_checksum(xsdt, xsdt->header.length) != 0)
      return NULL;
    sdt_ptr = xsdt->other_sdts;
    entry_size = sizeof(uint64_t);
    num_entries = (xsdt->header.length - sizeof(sdt_header_t)) / entry_size;
  } else {
    rsdt_t *rsdt = (rsdt_t *)rsdp->rsdt_addr;
    if (acpi_checksum(rsdt, rsdt->header.length) != 0)
      return NULL;
    sdt_ptr = rsdt->other_sdts;
    entry_size = sizeof(uint32_t);
    num_entries = (rsdt->header.length - sizeof(sdt_header_t)) / entry_size;
  }

  for (uint32_t i = 0; i < num_entries; i++) {
    uint64_t table_addr = (rsdp->revision >= 2) ? ((uint64_t *)sdt_ptr)[i]
                                                : ((uint32_t *)sdt_ptr)[i];
    sdt_header_t *header = (sdt_header_t *)(uintptr_t)table_addr;
    if (memcmp(header->sig, sig, 4) == 0 &&
        acpi_checksum(header, header->length) == 0) {
      return header;
    }
  }
  return NULL;
}

static void acpi_parse_fadt(fadt_t *fadt) {
  if (fadt == NULL) {
    log(LOG_ERR, "ACPI: FADT parsing failed - table not found.");
    return;
  }

  uint64_t dsdt_addr;
  uint64_t firmware_ctrl;
  uint64_t pm1a_cnt_addr;
  uint8_t address_space_id = 1;

  if (fadt->header.rev >= 3) {
    dsdt_addr = (fadt->x_dsdt_addr != 0) ? fadt->x_dsdt_addr : fadt->dsdt_addr;
    firmware_ctrl = (fadt->x_firmware_ctrl != 0) ? fadt->x_firmware_ctrl
                                                 : fadt->firmware_ctrl;
    if (fadt->x_pm1a_cnt_blk.address != 0) {
      pm1a_cnt_addr = fadt->x_pm1a_cnt_blk.address;
      address_space_id = fadt->x_pm1a_cnt_blk.address_space_id;
    } else {
      pm1a_cnt_addr = fadt->pm1a_cnt_blk;
    }
  } else {
    dsdt_addr = fadt->dsdt_addr;
    firmware_ctrl = fadt->firmware_ctrl;
    pm1a_cnt_addr = fadt->pm1a_cnt_blk;
  }

  log(LOG_INFO, "ACPI FADT: Preferred PM Profile: %d, SCI Interrupt: %d",
      fadt->preferred_pm_profile, fadt->sci_int);
  log(LOG_INFO,
      "ACPI FADT: SMI Command Port: 0x%x, ACPI Enable Value: 0x%x, ACPI "
      "Disable Value: 0x%x",
      fadt->smi_cmd, fadt->acpi_enable, fadt->acpi_disable);
  log(LOG_INFO, "ACPI FADT: DSDT Address: 0x%llx, FACS Address: 0x%llx",
      dsdt_addr, firmware_ctrl);
  log(LOG_INFO,
      "ACPI FADT: PM1 Event Length: %d, PM1 Control Length: %d, PM Timer "
      "Length: %d",
      fadt->pm1_evt_len, fadt->pm1_cnt_len, fadt->pm_tmr_len);
  log(LOG_INFO,
      "ACPI FADT: Feature Flags: 0x%x, IA-PC Boot Arch: 0x%x, ARM Boot Arch: "
      "0x%x",
      fadt->flags, fadt->iapc_boot_arch, fadt->arm_boot_arch);
  log(LOG_INFO,
      "ACPI FADT: Reset Register - Address Space: %d, Address: 0x%llx, Reset "
      "Value: 0x%x",
      fadt->reset_reg.address_space_id, fadt->reset_reg.address,
      fadt->reset_value);

  if (fadt->header.rev >= 3) {
    log(LOG_INFO,
        "ACPI FADT: Hypervisor Vendor ID: 0x%llx, FADT Minor Version: %d",
        fadt->hypervisor_vendor_id, fadt->fadt_minor_version);
  }

  if (fadt->pm1_cnt_len < 2) {
    log(LOG_WARN, "ACPI: Invalid PM1_CNT length; enabling skipped.");
    return;
  }

  if (pm1a_cnt_addr != 0) {
    if (address_space_id == 1) {
      uint16_t pm1_cnt = inw((uint16_t)pm1a_cnt_addr);
      if (pm1_cnt & 0x1) {
        log(LOG_INFO, "ACPI: Already enabled (SCI_EN set).");
        return;
      }

      if (fadt->smi_cmd == 0) {
        log(LOG_WARN, "ACPI: SMI_CMD invalid (zero); cannot enable.");
        return;
      }

      outb(fadt->smi_cmd, fadt->acpi_enable);
      log(LOG_INFO,
          "ACPI: Enable command sent to SMI_CMD (0x%x) with value 0x%x.",
          fadt->smi_cmd, fadt->acpi_enable);

      uint32_t timeout = 1000000;
      while (timeout--) {
        pm1_cnt = inw((uint16_t)pm1a_cnt_addr);
        if (pm1_cnt & 0x1) {
          log(LOG_INFO, "ACPI: Successfully enabled (SCI_EN set).");
          return;
        }
      }
      log(LOG_ERR, "ACPI: Failed to enable (SCI_EN not set after timeout).");
    } else if (address_space_id == 0) {
      volatile uint16_t *pm1_cnt_ptr = (volatile uint16_t *)pm1a_cnt_addr;
      uint16_t pm1_cnt = *pm1_cnt_ptr;
      if (pm1_cnt & 0x1) {
        log(LOG_INFO, "ACPI: Already enabled (SCI_EN set).");
        return;
      }

      if (fadt->smi_cmd == 0) {
        log(LOG_WARN, "ACPI: SMI_CMD invalid (zero); cannot enable.");
        return;
      }

      outb(fadt->smi_cmd, fadt->acpi_enable);
      log(LOG_INFO,
          "ACPI: Enable command sent to SMI_CMD (0x%x) with value 0x%x.",
          fadt->smi_cmd, fadt->acpi_enable);

      uint32_t timeout = 1000000;
      while (timeout--) {
        pm1_cnt = *pm1_cnt_ptr;
        if (pm1_cnt & 0x1) {
          log(LOG_INFO, "ACPI: Successfully enabled (SCI_EN set).");
          return;
        }
      }
      log(LOG_ERR, "ACPI: Failed to enable (SCI_EN not set after timeout).");
    } else {
      log(LOG_WARN, "ACPI: Unsupported address space ID; enabling skipped.");
    }
  } else {
    log(LOG_WARN, "ACPI: PM1_CNT address invalid; enabling skipped.");
  }
}

void acpi_init(void) {
  rsdp_t *rsdp = acpi_find_rsdp();
  if (rsdp == NULL) {
    log(LOG_ERR, "ACPI: RSDP not found.");
    return;
  }
  log(LOG_INFO, "ACPI: RSDP found at 0x%x, Revision: %d, OEM: %s",
      (uint32_t)rsdp, rsdp->revision, rsdp->oemid);

  void *sdt = NULL;
  uint32_t entry_size, num_tables;

  if (rsdp->revision >= 2) {
    xsdp_t *xsdp = (xsdp_t *)rsdp;
    if (acpi_checksum(xsdp, xsdp->length) != 0) {
      log(LOG_ERR, "ACPI: Extended checksum invalid.");
      return;
    }
    xsdt_t *xsdt = (xsdt_t *)(uintptr_t)xsdp->xsdt_addr;
    if (acpi_checksum(xsdt, xsdt->header.length) != 0) {
      log(LOG_ERR, "ACPI: XSDT checksum invalid.");
      return;
    }
    sdt = xsdt->other_sdts;
    entry_size = sizeof(uint64_t);
    num_tables = (xsdt->header.length - sizeof(sdt_header_t)) / entry_size;
    log(LOG_INFO, "ACPI: XSDT at 0x%llx, %d tables available.", xsdp->xsdt_addr,
        num_tables);
  } else {
    rsdt_t *rsdt = (rsdt_t *)rsdp->rsdt_addr;
    if (acpi_checksum(rsdt, rsdt->header.length) != 0) {
      log(LOG_ERR, "ACPI: RSDT checksum invalid.");
      return;
    }
    sdt = rsdt->other_sdts;
    entry_size = sizeof(uint32_t);
    num_tables = (rsdt->header.length - sizeof(sdt_header_t)) / entry_size;
    log(LOG_INFO, "ACPI: RSDT at 0x%x, %d tables available.", rsdp->rsdt_addr,
        num_tables);
  }

  for (uint32_t i = 0; i < num_tables; i++) {
    uint64_t table_addr =
        (rsdp->revision >= 2) ? ((uint64_t *)sdt)[i] : ((uint32_t *)sdt)[i];
    sdt_header_t *header = (sdt_header_t *)(uintptr_t)table_addr;
    if (acpi_checksum(header, header->length) == 0) {
      log(LOG_INFO, "ACPI Table: %.4s (Rev %d, OEM: %.6s)", header->sig,
          header->rev, header->oemid);
    }
  }

  fadt_t *fadt = (fadt_t *)acpi_get_table("FACP");
  if (fadt != NULL) {
    acpi_parse_fadt(fadt);
  }

  madt_t *madt = (madt_t *)acpi_get_table("APIC");
  if (madt != NULL) {
    log(LOG_INFO, "ACPI: MADT found - Local APIC at 0x%x, Flags: 0x%x",
        madt->local_apic_addr, madt->flags);
  }
}
