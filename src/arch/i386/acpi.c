#include <arch/i386/acpi.h>
#include <limits.h>
#include <misc/logger.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <string.h>

#define RSDP_SIGNATURE "RSD PTR "
#define RSDP_SIGNATURE_LEN 8
#define RSDP_V1_LEN 20
#define EBDA_BASE 0x40E
#define EBDA_SIZE 1024
#define BIOS_AREA_START 0xE0000
#define BIOS_AREA_END 0x100000
#define ALIGNMENT_16 16
#define PAGE_SIZE 4096
#define TIMEOUT 1000000

typedef struct {
  rsdp_t *rsdp;
  void *sdt_ptr;
  u32 entry_size;
  u32 num_entries;
  sdt_header_t **tables;
} acpi_context_t;

static acpi_context_t acpi_ctx = {0};

static u8 acpi_checksum(const void *table, u32 length) {
  u8 sum = 0;
  const u8 *ptr = (const u8 *)table;
  while (length--) {
    sum += *ptr++;
  }
  return sum;
}

rsdp_t *acpi_find_rsdp(void) {
  // search EBDA
  u16 ebda_seg = *((const u16 *)EBDA_BASE);
  u32 ebda_addr = (u32)ebda_seg << 4;
  for (u32 ptr = ebda_addr; ptr < ebda_addr + EBDA_SIZE; ptr += ALIGNMENT_16) {
    if (memcmp((const void *)ptr, RSDP_SIGNATURE, RSDP_SIGNATURE_LEN) == 0) {
      if (acpi_checksum((const void *)ptr, RSDP_V1_LEN) == 0) {
        return (rsdp_t *)ptr;
      }
    }
  }
  // search BIOS area
  for (u32 ptr = BIOS_AREA_START; ptr < BIOS_AREA_END; ptr += ALIGNMENT_16) {
    if (memcmp((const void *)ptr, RSDP_SIGNATURE, RSDP_SIGNATURE_LEN) == 0) {
      if (acpi_checksum((const void *)ptr, RSDP_V1_LEN) == 0) {
        return (rsdp_t *)ptr;
      }
    }
  }
  return NULL;
}

sdt_header_t *acpi_get_table(const char *sig) {
  if (!acpi_ctx.rsdp) {
    return NULL;
  }

  for (u32 i = 0; i < acpi_ctx.num_entries; i++) {
    if (acpi_ctx.tables && acpi_ctx.tables[i]) {
      sdt_header_t *header = acpi_ctx.tables[i];
      if (memcmp(header->sig, sig, 4) == 0) {
        return header;
      }
    }
  }

  const u8 *sdt = (const u8 *)acpi_ctx.sdt_ptr;
  for (u32 i = 0; i < acpi_ctx.num_entries; i++, sdt += acpi_ctx.entry_size) {
    u64 table_addr = (acpi_ctx.entry_size == sizeof(u64)) ? *(const u64 *)sdt
                                                          : *(const u32 *)sdt;
    if (table_addr >> 32) {
      log(LOG_WARN,
          "ACPI: Skipping table at 0x%llx (exceeds 32-bit address space).",
          table_addr);
      continue;
    }
    sdt_header_t *header = (sdt_header_t *)(uintptr_t)table_addr;
    if (acpi_checksum(header, header->length) == 0 &&
        memcmp(header->sig, sig, 4) == 0) {
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

  u64 dsdt_addr = (fadt->header.rev >= 3 && fadt->x_dsdt_addr)
                      ? fadt->x_dsdt_addr
                      : fadt->dsdt_addr;
  u64 firmware_ctrl = (fadt->header.rev >= 3 && fadt->x_firmware_ctrl)
                          ? fadt->x_firmware_ctrl
                          : fadt->firmware_ctrl;
  u64 pm1a_cnt_addr = 0;
  u8 address_space_id = 1; // I/O space

  if (fadt->header.rev >= 3 && fadt->x_pm1a_cnt_blk.address) {
    pm1a_cnt_addr = fadt->x_pm1a_cnt_blk.address;
    address_space_id = fadt->x_pm1a_cnt_blk.address_space_id;
  } else {
    pm1a_cnt_addr = fadt->pm1a_cnt_blk;
  }

  log(LOG_INFO,
      "ACPI FADT: Preferred PM Profile: %d, SCI Interrupt: %d, SMI Command "
      "Port: 0x%x",
      fadt->preferred_pm_profile, fadt->sci_int, fadt->smi_cmd);
  log(LOG_INFO,
      "ACPI FADT: ACPI Enable/Disable: 0x%x/0x%x, DSDT: 0x%llx, FACS: 0x%llx",
      fadt->acpi_enable, fadt->acpi_disable, dsdt_addr, firmware_ctrl);
  log(LOG_INFO,
      "ACPI FADT: PM1 Event/Control/Timer Lengths: %d/%d/%d, Flags: 0x%x",
      fadt->pm1_evt_len, fadt->pm1_cnt_len, fadt->pm_tmr_len, fadt->flags);
  log(LOG_INFO,
      "ACPI FADT: IA-PC/ARM Boot Arch: 0x%x/0x%x, Reset Reg (Space: %d, Addr: "
      "0x%llx, Val: 0x%x)",
      fadt->iapc_boot_arch, fadt->arm_boot_arch,
      fadt->reset_reg.address_space_id, fadt->reset_reg.address,
      fadt->reset_value);

  if (fadt->header.rev >= 3) {
    log(LOG_INFO, "ACPI FADT: Hypervisor Vendor ID: 0x%llx, Minor Version: %d",
        fadt->hypervisor_vendor_id, fadt->fadt_minor_version);
  }

  if (fadt->pm1_cnt_len < 2) {
    log(LOG_WARN, "ACPI: Invalid PM1_CNT length; enabling skipped.");
    return;
  }

  if (pm1a_cnt_addr == 0 || (pm1a_cnt_addr >> 32)) {
    log(LOG_WARN,
        "ACPI: Invalid or unsupported PM1_CNT address (0x%llx); enabling "
        "skipped.",
        pm1a_cnt_addr);
    return;
  }

  u16 pm1_cnt = 0;
  if (address_space_id == 1) {
    pm1_cnt = inw((u16)pm1a_cnt_addr);
  } else if (address_space_id == 0) {
    if ((pm1a_cnt_addr & 1) != 0) {
      log(LOG_WARN,
          "ACPI: PM1_CNT address not 16-bit aligned; enabling skipped.");
      return;
    }
    if (pm1a_cnt_addr < 0x1000 ||
        pm1a_cnt_addr >= (u64)0xFFFFFFFF - PAGE_SIZE) {
      log(LOG_WARN,
          "ACPI: PM1_CNT address out of valid range; enabling skipped.");
      return;
    }
    u32 phys_addr = vmm_get_physical_addr((u32)pm1a_cnt_addr);
    if (phys_addr == 0) {
      u32 flags = 0x3;
      vmm_map_page((u32)pm1a_cnt_addr, phys_addr, flags);
      log(LOG_WARN,
          "ACPI: PM1_CNT not mapped; attempt mapping if VMM supports.");
      return;
    }
    volatile u16 *pm1_cnt_ptr = (volatile u16 *)(uintptr_t)pm1a_cnt_addr;
    pm1_cnt = *pm1_cnt_ptr;
  } else {
    log(LOG_WARN, "ACPI: Unsupported address space ID (%d); enabling skipped.",
        address_space_id);
    return;
  }

  if (pm1_cnt & 0x1) {
    log(LOG_INFO, "ACPI: Already enabled (SCI_EN set).");
    return;
  }

  if (fadt->smi_cmd == 0) {
    log(LOG_WARN, "ACPI: SMI_CMD invalid (zero); cannot enable.");
    return;
  }

  outb(fadt->smi_cmd, fadt->acpi_enable);
  log(LOG_INFO, "ACPI: Enable command sent to SMI_CMD (0x%x) with value 0x%x.",
      fadt->smi_cmd, fadt->acpi_enable);

  u32 timeout = TIMEOUT;
  while (timeout--) {
    if (address_space_id == 1) {
      pm1_cnt = inw((u16)pm1a_cnt_addr);
    } else {
      volatile u16 *pm1_cnt_ptr = (volatile u16 *)(uintptr_t)pm1a_cnt_addr;
      pm1_cnt = *pm1_cnt_ptr;
    }
    if (pm1_cnt & 0x1) {
      log(LOG_OKAY, "ACPI: Successfully enabled (SCI_EN set).");
      return;
    }
  }
  log(LOG_ERR, "ACPI: Failed to enable (SCI_EN not set after timeout).");
}

void acpi_init(void) {
  acpi_ctx.rsdp = acpi_find_rsdp();
  if (acpi_ctx.rsdp == NULL) {
    log(LOG_ERR, "ACPI: RSDP not found.");
    return;
  }
  log(LOG_INFO, "ACPI: RSDP found at 0x%x, Revision: %d, OEM: %s",
      (u32)acpi_ctx.rsdp, acpi_ctx.rsdp->revision, acpi_ctx.rsdp->oemid);

  u32 sdt_phys;
  bool is_xsdt = false;
  void *sdt_header_raw;

  if (acpi_ctx.rsdp->revision >= 2) {
    xsdp_t *xsdp = (xsdp_t *)acpi_ctx.rsdp;
    if (acpi_checksum(xsdp, xsdp->length) != 0) {
      log(LOG_ERR, "ACPI: Extended checksum invalid.");
      return;
    }
    sdt_phys = (u32)xsdp->xsdt_addr;
    if ((u64)xsdp->xsdt_addr > UINT32_MAX) {
      log(LOG_ERR, "ACPI: XSDT exceeds 32-bit address space.");
      return;
    }
    is_xsdt = true;
  } else {
    sdt_phys = acpi_ctx.rsdp->rsdt_addr;
  }

  kernel_status_t status = vmm_map_if_not_mapped(sdt_phys, PAGE_SIZE);
  if (status != KERNEL_OK) {
    log(LOG_ERR, "ACPI: Failed to map SDT header (status: %d).", status);
    return;
  }

  sdt_header_raw = (void *)(uintptr_t)sdt_phys;
  sdt_header_t *sdt_header = (sdt_header_t *)sdt_header_raw;

  const char *expected_sig = is_xsdt ? "XSDT" : "RSDT";
  if (memcmp(sdt_header->sig, expected_sig, 4) != 0) {
    log(LOG_ERR, "ACPI: Invalid SDT signature (expected %s).", expected_sig);
    return;
  }

  u32 sdt_length = sdt_header->length;

  status = vmm_map_if_not_mapped(sdt_phys, sdt_length);
  if (status != KERNEL_OK) {
    log(LOG_ERR, "ACPI: Failed to map full SDT (status: %d).", status);
    return;
  }

  if (acpi_checksum(sdt_header_raw, sdt_length) != 0) {
    log(LOG_ERR, "ACPI: SDT checksum invalid.");
    return;
  }

  status = vmm_map_if_not_mapped(sdt_phys, sdt_length);
  if (status != KERNEL_OK) {
    log(LOG_ERR, "ACPI: Failed to map full SDT (status: %d).", status);
    return;
  }

  if (acpi_checksum(sdt_header_raw, sdt_length) != 0) {
    log(LOG_ERR, "ACPI: SDT checksum invalid.");
    return;
  }

  if (is_xsdt) {
    xsdt_t *xsdt = (xsdt_t *)sdt_header_raw;
    acpi_ctx.sdt_ptr = xsdt->other_sdts;
    acpi_ctx.entry_size = sizeof(u64);
    acpi_ctx.num_entries =
        (sdt_length - sizeof(sdt_header_t)) / acpi_ctx.entry_size;
    log(LOG_INFO, "ACPI: XSDT at 0x%x, %d tables available.", sdt_phys,
        acpi_ctx.num_entries);
  } else {
    rsdt_t *rsdt = (rsdt_t *)sdt_header_raw;
    acpi_ctx.sdt_ptr = rsdt->other_sdts;
    acpi_ctx.entry_size = sizeof(u32);
    acpi_ctx.num_entries =
        (sdt_length - sizeof(sdt_header_t)) / acpi_ctx.entry_size;
    log(LOG_INFO, "ACPI: RSDT at 0x%x, %d tables available.", sdt_phys,
        acpi_ctx.num_entries);
  }

#define MAX_TABLES 32
  acpi_ctx.tables =
      (sdt_header_t **)kcalloc(MAX_TABLES, sizeof(sdt_header_t *));
  if (!acpi_ctx.tables) {
    log(LOG_ERR, "ACPI: Failed to allocate table cache.");
    return;
  }

  const u8 *sdt = (const u8 *)acpi_ctx.sdt_ptr;
  u32 cached_count = 0;
  for (u32 i = 0; i < acpi_ctx.num_entries && cached_count < MAX_TABLES;
       i++, sdt += acpi_ctx.entry_size) {
    u64 table_phys = (acpi_ctx.entry_size == sizeof(u64)) ? *(const u64 *)sdt
                                                          : *(const u32 *)sdt;
    if (table_phys > UINT32_MAX) {
      log(LOG_WARN,
          "ACPI: Skipping table at 0x%llx (exceeds 32-bit address space).",
          table_phys);
      continue;
    }

    status = vmm_map_if_not_mapped((u32)table_phys, PAGE_SIZE);
    if (status != KERNEL_OK) {
      log(LOG_WARN, "ACPI: Failed to map table header at 0x%x (status: %d).",
          (u32)table_phys, status);
      continue;
    }

    sdt_header_t *header = (sdt_header_t *)(uintptr_t)table_phys;
    if (acpi_checksum(header, sizeof(sdt_header_t)) != 0) {
      continue;
    }

    u32 table_length = header->length;

    status = vmm_map_if_not_mapped((u32)table_phys, table_length);
    if (status != KERNEL_OK) {
      log(LOG_WARN, "ACPI: Failed to map full table at 0x%x (status: %d).",
          (u32)table_phys, status);
      continue;
    }

    if (acpi_checksum(header, table_length) == 0) {
      acpi_ctx.tables[cached_count++] = header;
      log(LOG_INFO, "ACPI Table: %.4s (Rev %d, OEM: %.6s)", header->sig,
          header->rev, header->oemid);
    }
  }

  fadt_t *fadt = (fadt_t *)acpi_get_table("FACP");
  if (fadt) {
    acpi_parse_fadt(fadt);
  }

  madt_t *madt = (madt_t *)acpi_get_table("APIC");
  if (madt) {
    log(LOG_OKAY, "ACPI: MADT found - Local APIC at 0x%x, Flags: 0x%x",
        madt->local_apic_addr, madt->flags);
  }
}
