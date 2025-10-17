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

static inline u16 read16(uintptr_t addr) {
  return *(volatile u16 *)addr;
}

rsdp_t *acpi_find_rsdp(void) {
  // search EBDA
  u16 ebda_seg = read16(EBDA_BASE);
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

static void acpi_parse_madt(madt_t *madt) {
  if (madt == NULL) {
    log(LOG_ERR, "ACPI: MADT parsing failed - table not found.");
    return;
  }

  u8 *ptr = (u8 *)madt + sizeof(sdt_header_t) + 8;
  u8 *end = (u8 *)madt + madt->header.length;
  u32 entry_count = 0;

  log(LOG_INFO, "ACPI: Parsing MADT entries (LAPIC Addr: 0x%x, Flags: 0x%x)",
      madt->local_apic_addr, madt->flags);

  while (ptr + 1 < end) {
    u8 type = *ptr;
    u8 length = *(ptr + 1);

    if (length < 2 || ptr + length > end) {
      log(LOG_WARN,
          "ACPI: Invalid MADT entry at offset 0x%lx (type: %u, len: %u); "
          "stopping parse.",
          (uintptr_t)(ptr - (u8 *)madt), type, length);
      break;
    }

    switch (type) {
    case 0:
      if (length >= 8) {
        u8 processor_id = *(ptr + 2);
        u8 apic_id = *(ptr + 3);
        u32 flags = *(u32 *)(ptr + 4);
        log(LOG_INFO,
            "  MADT Entry %u: LAPIC (Proc ID: %u, APIC ID: %u, Flags: 0x%x)",
            entry_count, processor_id, apic_id, flags);
      }
      break;
    case 1:
      if (length >= 12) {
        u8 ioapic_id = *(ptr + 2);
        u32 address = *(u32 *)(ptr + 4);
        u32 gsi_base = *(u32 *)(ptr + 8);
        log(LOG_INFO,
            "  MADT Entry %u: I/O APIC (ID: %u, Addr: 0x%x, GSI BASE: %u)",
            entry_count, ioapic_id, address, gsi_base);
      }
      break;
    case 2:
      if (length >= 10) {
        u8 bus = *(ptr + 2);
        u8 source = *(ptr + 3);
        u32 gsi = *(u32 *)(ptr + 4);
        u16 flags = *(u16 *)(ptr + 8);
        log(LOG_INFO,
            "  MADT Entry %u: IRQ Source Override (Bus: %u, Source: %u, GSI: "
            "%u, Flags: 0x%x)",
            entry_count, bus, source, gsi, flags);
      }
      break;
    case 3:
      if (length >= 10) {
        u8 nmi_source = *(ptr + 2);
        u8 reserved = *(ptr + 3);
        u16 flags = *(u16 *)(ptr + 4);
        u32 gsi = *(u32 *)(ptr + 6);
        log(LOG_INFO,
            "  MADT Entry %u: I/O APIC NMI (Source: %u, Reserved: 0x%x, Flags: "
            "0x%x, GSI: %u)",
            entry_count, nmi_source, reserved, flags, gsi);
      }
      break;
    case 4:
      if (length >= 6) {
        u8 processor_id = *(ptr + 2);
        u16 flags = *(u16 *)(ptr + 3);
        u8 lint = *(ptr + 5);
        log(LOG_INFO,
            "  MADT Entry %u: Local APIC NMI (Proc ID: %u, Flags: 0x%x, LINT: "
            "%u)",
            entry_count, processor_id, flags, lint);
      }
      break;
    case 5:
      if (length >= 12) {
        u16 reserved = *(u16 *)(ptr + 2);
        u64 address = *(u64 *)(ptr + 4);
        log(LOG_INFO,
            "  MADT Entry %u: Local APIC Addr Override (Reserved: 0x%x, Addr: "
            "0x%llx)",
            entry_count, reserved, address);
      }
      break;
    case 6:
      if (length >= 16) {
        u8 io_sapic_id = *(ptr + 2);
        u8 reserved = *(ptr + 3);
        u32 gsi_base = *(u32 *)(ptr + 4);
        u64 address = *(u64 *)(ptr + 8);
        log(LOG_INFO,
            "  MADT Entry %u: I/O SAPIC (ID: %u, Reserved: 0x%x, GSI BASE: %u, "
            "Addr: 0x%llx)",
            entry_count, io_sapic_id, reserved, gsi_base, address);
      }
      break;
    case 7:
      if (length >= 16) {
        u8 processor_id = *(ptr + 2);
        u8 sapic_id = *(ptr + 3);
        u8 sapic_eid = *(ptr + 4);
        u32 flags = *(u32 *)(ptr + 5);
        u32 uid_value = *(u32 *)(ptr + 9);
        log(LOG_INFO,
            "  MADT Entry %u: Local SAPIC (Proc ID: %u, SAPIC ID: %u, EID: %u, "
            "Flags: 0x%x, UID Value: %u)",
            entry_count, processor_id, sapic_id, sapic_eid, flags, uid_value);
      }
      break;
    case 8:
      if (length >= 16) {
        u16 flags = *(u16 *)(ptr + 2);
        u8 interrupt_type = *(ptr + 4);
        u8 processor_id = *(ptr + 5);
        u8 processor_eid = *(ptr + 6);
        u8 io_sapic_vector = *(ptr + 7);
        u32 gsi = *(u32 *)(ptr + 8);
        u32 platform_flags = *(u32 *)(ptr + 12);
        log(LOG_INFO,
            "  MADT Entry %u: Platform Int Src (Flags: 0x%x, Type: %u, Proc "
            "ID: %u, EID: %u, Vector: %u, GSI: %u, Plat Flags: 0x%x)",
            entry_count, flags, interrupt_type, processor_id, processor_eid,
            io_sapic_vector, gsi, platform_flags);
      }
      break;
    case 9:
      if (length >= 16) {
        u16 reserved = *(u16 *)(ptr + 2);
        u32 x2apic_id = *(u32 *)(ptr + 4);
        u32 flags = *(u32 *)(ptr + 8);
        u32 acpi_id = *(u32 *)(ptr + 12);
        log(LOG_INFO,
            "  MADT Entry %u: Local x2APIC (Reserved: 0x%x, x2APIC ID: %u, "
            "Flags: 0x%x, ACPI ID: %u)",
            entry_count, reserved, x2apic_id, flags, acpi_id);
      }
      break;
    case 10:
      if (length >= 12) {
        u16 flags = *(u16 *)(ptr + 2);
        u32 acpi_processor_uid = *(u32 *)(ptr + 4);
        u8 lint = *(ptr + 8);
        u8 reserved = *(ptr + 9);
        log(LOG_INFO,
            "  MADT Entry %u: Local x2APIC NMI (Flags: 0x%x, ACPI UID: %u, "
            "LINT: %u, Reserved: 0x%x)",
            entry_count, flags, acpi_processor_uid, lint, reserved);
      }
      break;
    case 11:
      if (length >= 82) {
        u32 cpu_interface_number = *(u32 *)(ptr + 2);
        u32 acpi_processor_uid = *(u32 *)(ptr + 6);
        u32 flags = *(u32 *)(ptr + 10);
        u32 parking_protocol_version = *(u32 *)(ptr + 14);
        u32 performance_interrupt_gsiv = *(u32 *)(ptr + 18);
        u64 parked_address = *(u64 *)(ptr + 22);
        u64 physical_base_address = *(u64 *)(ptr + 30);
        u64 gicv = *(u64 *)(ptr + 38);
        u64 gich = *(u64 *)(ptr + 46);
        u32 vgic_maintenance_interrupt = *(u32 *)(ptr + 54);
        u64 gicr_base_address = *(u64 *)(ptr + 58);
        u64 mpidr = *(u64 *)(ptr + 66);
        u8 processor_power_efficiency_class = *(ptr + 74);
        u16 spe_overflow_interrupt = *(u16 *)(ptr + 75);
        u16 trbe_interrupt = *(u16 *)(ptr + 77);
        log(LOG_INFO,
            "  MADT Entry %u: GICC (CPU If Num: %u, UID: %u, Flags: 0x%x, "
            "Parking Ver: %u, Perf GSIV: %u, Parked Addr: 0x%llx, Phys Base: "
            "0x%llx, GICV: 0x%llx, GICH: 0x%llx, VGIC Maint: %u, GICR Base: "
            "0x%llx, MPIDR: 0x%llx, Pwr Eff Class: %u, SPE GSIV: %u, TRBE "
            "GSIV: %u)",
            entry_count, cpu_interface_number, acpi_processor_uid, flags,
            parking_protocol_version, performance_interrupt_gsiv,
            parked_address, physical_base_address, gicv, gich,
            vgic_maintenance_interrupt, gicr_base_address, mpidr,
            processor_power_efficiency_class, spe_overflow_interrupt,
            trbe_interrupt);
      }
      break;
    case 12:
      if (length >= 24) {
        u32 gic_id = *(u32 *)(ptr + 2);
        u64 physical_base_address = *(u64 *)(ptr + 6);
        u8 gic_version = *(ptr + 14);
        log(LOG_INFO,
            "  MADT Entry %u: GICD (GIC ID: %u, Phys Base: 0x%llx, Version: "
            "%u)",
            entry_count, gic_id, physical_base_address, gic_version);
      }
      break;
    case 13:
      if (length >= 24) {
        u32 msi_frame_id = *(u32 *)(ptr + 2);
        u64 physical_base_address = *(u64 *)(ptr + 6);
        u32 flags = *(u32 *)(ptr + 14);
        u16 spi_count = *(u16 *)(ptr + 18);
        u16 spi_base = *(u16 *)(ptr + 20);
        log(LOG_INFO,
            "  MADT Entry %u: GIC MSI Frame (ID: %u, Phys Base: 0x%llx, Flags: "
            "0x%x, SPI Count: %u, SPI Base: %u)",
            entry_count, msi_frame_id, physical_base_address, flags, spi_count,
            spi_base);
      }
      break;
    case 14:
      if (length >= 16) {
        u64 discovery_range_base = *(u64 *)(ptr + 2);
        u32 discovery_range_length = *(u32 *)(ptr + 10);
        u16 reserved = *(u16 *)(ptr + 14);
        log(LOG_INFO,
            "  MADT Entry %u: GICR (Discovery Base: 0x%llx, Length: %u, "
            "Reserved: 0x%x)",
            entry_count, discovery_range_base, discovery_range_length,
            reserved);
      }
      break;
    case 15:
      if (length >= 20) {
        u32 its_id = *(u32 *)(ptr + 2);
        u64 physical_base_address = *(u64 *)(ptr + 6);
        log(LOG_INFO, "  MADT Entry %u: GIC ITS (ID: %u, Phys Base: 0x%llx)",
            entry_count, its_id, physical_base_address);
      }
      break;
    case 16:
      if (length >= 16) {
        u16 mailbox_version = *(u16 *)(ptr + 2);
        u16 reserved = *(u16 *)(ptr + 4);
        u64 mailbox_address = *(u64 *)(ptr + 6);
        log(LOG_INFO,
            "  MADT Entry %u: MP Wakeup (Mailbox Ver: %u, Reserved: 0x%x, "
            "Addr: 0x%llx)",
            entry_count, mailbox_version, reserved, mailbox_address);
      }
      break;
    case 17:
      if (length >= 12) {
        u8 version = *(ptr + 2);
        u32 acpi_processor_id = *(u32 *)(ptr + 3);
        u32 physical_processor_id = *(u32 *)(ptr + 7);
        u32 flags = *(u32 *)(ptr + 11);
        log(LOG_INFO,
            "  MADT Entry %u: Core PIC (Ver: %u, ACPI Proc ID: %u, Phys Proc "
            "ID: %u, Flags: 0x%x)",
            entry_count, version, acpi_processor_id, physical_processor_id,
            flags);
      }
      break;
    case 18:
      if (length >= 16) {
        u8 version = *(ptr + 2);
        u64 base_address = *(u64 *)(ptr + 3);
        u16 size = *(u16 *)(ptr + 11);
        u16 cascade_vector = *(u16 *)(ptr + 13);
        log(LOG_INFO,
            "  MADT Entry %u: LIO PIC (Ver: %u, Base Addr: 0x%llx, Size: %u, "
            "Cascade Vec: %u; Mapping: variable)",
            entry_count, version, base_address, size, cascade_vector);
      }
      break;
    case 19:
      if (length >= 16) {
        u8 version = *(ptr + 2);
        u64 base_address = *(u64 *)(ptr + 3);
        u16 size = *(u16 *)(ptr + 11);
        u64 cascade_vector = *(u64 *)(ptr + 13);
        log(LOG_INFO,
            "  MADT Entry %u: HT PIC (Ver: %u, Base Addr: 0x%llx, Size: %u, "
            "Cascade Vec: 0x%llx)",
            entry_count, version, base_address, size, cascade_vector);
      }
      break;
    case 20:
      if (length >= 12) {
        u8 version = *(ptr + 2);
        u8 cascade_vector = *(ptr + 3);
        u8 node = *(ptr + 4);
        u64 node_map = *(u64 *)(ptr + 5);
        log(LOG_INFO,
            "  MADT Entry %u: EIO PIC (Ver: %u, Cascade Vec: %u, Node: %u, "
            "Node Map: 0x%llx)",
            entry_count, version, cascade_vector, node, node_map);
      }
      break;
    case 21:
      if (length >= 16) {
        u8 version = *(ptr + 2);
        u64 message_address = *(u64 *)(ptr + 3);
        u32 start = *(u32 *)(ptr + 11);
        u32 count = *(u32 *)(ptr + 15);
        log(LOG_INFO,
            "  MADT Entry %u: MSI PIC (Ver: %u, Msg Addr: 0x%llx, Start Vec: "
            "%u, Count: %u)",
            entry_count, version, message_address, start, count);
      }
      break;
    case 22:
      if (length >= 16) {
        u8 version = *(ptr + 2);
        u64 base_address = *(u64 *)(ptr + 3);
        u16 size = *(u16 *)(ptr + 11);
        u16 hardware_id = *(u16 *)(ptr + 13);
        u16 gsi_base = *(u16 *)(ptr + 15);
        log(LOG_INFO,
            "  MADT Entry %u: BIO PIC (Ver: %u, Base Addr: 0x%llx, Size: %u, "
            "HW ID: %u, GSI Base: %u)",
            entry_count, version, base_address, size, hardware_id, gsi_base);
      }
      break;
    case 23:
      if (length >= 16) {
        u8 version = *(ptr + 2);
        u64 base_address = *(u64 *)(ptr + 3);
        u16 size = *(u16 *)(ptr + 11);
        u16 cascade_vector = *(u16 *)(ptr + 13);
        log(LOG_INFO,
            "  MADT Entry %u: LPC PIC (Ver: %u, Base Addr: 0x%llx, Size: %u, "
            "Cascade Vec: %u)",
            entry_count, version, base_address, size, cascade_vector);
      }
      break;
    default:
      log(LOG_WARN,
          "  MADT Entry %u: Unknown type %u (length: %u); skipping parse.",
          entry_count, type, length);
      break;
    }

    ptr += length;
    entry_count++;
  }

  log(LOG_OKAY, "ACPI: MADT parsing complete (%u entries processed).",
      entry_count);
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

  log(LOG_INFO, "ACPI: RSDT/XSDT Entries:");
  const u8 *sdt_entries = (const u8 *)acpi_ctx.sdt_ptr;
  for (u32 i = 0; i < acpi_ctx.num_entries;
       i++, sdt_entries += acpi_ctx.entry_size) {
    u64 table_addr = (acpi_ctx.entry_size == sizeof(u64))
                         ? *(const u64 *)sdt_entries
                         : *(const u32 *)sdt_entries;
    log(LOG_INFO, "  Entry %u: 0x%llx", i, table_addr);
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
    log(LOG_OKAY, "ACPI: MADT found - LAPIC at 0x%x, Flags: 0x%x",
        madt->local_apic_addr, madt->flags);
    acpi_parse_madt(madt);
  }
}
