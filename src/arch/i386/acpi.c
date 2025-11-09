/*
 * ACPI implementation for FrostixOS (i386)
 */
#include <arch/i386/acpi.h>
#include <limits.h>
#include <misc/logger.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <string.h>
#include <stdint.h>

#define RSDP_SIGNATURE	    "RSD PTR "
#define RSDP_SIGNATURE_LEN  8
#define RSDP_V1_LEN	    20
#define EBDA_BASE	    0x40E
#define EBDA_SIZE	    1024
#define BIOS_AREA_START     0xE0000UL
#define BIOS_AREA_END	    0x100000UL
#define ALIGNMENT_16	    16
#define TIMEOUT		    1000000U
#define MAX_TABLES	    32

typedef struct {
	rsdp_t *rsdp;
	void *sdt_ptr;
	u32 entry_size;
	u32 num_entries;
	sdt_header_t **tables;
} acpi_context_t;

static acpi_context_t acpi_ctx = { 0 };

/* Calculate ACPI checksum for the given table */
static u8 acpi_checksum(const void *table, u32 length)
{
	const u8 *p = (const u8 *)table;
	u8 sum = 0;

	while (length--)
		sum += *p++;

	return sum;
}

/* Read a 16-bit value from the specified memory address (typically BIOS/EBDA) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static inline u16 read16(uintptr_t addr) {
    return *(volatile const u16 *)addr;
}
#pragma GCC diagnostic pop

/*
 * Search for the Root System Description Pointer (RSDP)
 * Search EBDA then BIOS area, aligned at 16 bytes.
 */
rsdp_t *acpi_find_rsdp(void)
{
	/* EBDA: segment is stored at 0x40E */
	u16 ebda_seg = read16(EBDA_BASE);
	u32 ebda_addr = (u32)ebda_seg << 4;
	if (ebda_addr) {
		for (uintptr_t ptr = ebda_addr;
		     ptr + RSDP_V1_LEN <= ebda_addr + EBDA_SIZE;
		     ptr += ALIGNMENT_16) {
			if (memcmp((const void *)ptr, RSDP_SIGNATURE,
				   RSDP_SIGNATURE_LEN) == 0) {
				if (acpi_checksum((const void *)ptr,
						  RSDP_V1_LEN) == 0)
					return (rsdp_t *)ptr;
			}
		}
	}

	/* BIOS area: 0xE0000 - 0xFFFFF */
	for (uintptr_t ptr = BIOS_AREA_START;
	     ptr + RSDP_V1_LEN <= BIOS_AREA_END;
	     ptr += ALIGNMENT_16) {
		if (memcmp((const void *)ptr, RSDP_SIGNATURE,
			   RSDP_SIGNATURE_LEN) == 0) {
			if (acpi_checksum((const void *)ptr, RSDP_V1_LEN) == 0)
				return (rsdp_t *)ptr;
		}
	}

	return NULL;
}

/* Lookup an ACPI table by signature. Can use cached headers or map on-demand. */
sdt_header_t *acpi_get_table(const char *sig)
{
	u32 i;

	if (!acpi_ctx.rsdp || !sig)
		return NULL;

	/* First check cached pointers */
	for (i = 0; i < acpi_ctx.num_entries; i++) {
		if (acpi_ctx.tables && acpi_ctx.tables[i]) {
			sdt_header_t *h = acpi_ctx.tables[i];
			if (memcmp(h->sig, sig, 4) == 0)
				return h;
		}
	}

	/* Fall back to walking the RSDT/XSDT entries */
	const u8 *entry_ptr = (const u8 *)acpi_ctx.sdt_ptr;
	for (i = 0; i < acpi_ctx.num_entries; i++, entry_ptr += acpi_ctx.entry_size) {
		u64 table_addr = (acpi_ctx.entry_size == sizeof(u64))
					 ? *(const u64 *)entry_ptr
					 : *(const u32 *)entry_ptr;

		/* ignore addresses beyond 32-bit for this kernel */
		if (table_addr > UINT32_MAX) {
			log(LOG_WARN, "ACPI: skipping table at 0x%llx (32-bit kernel)",
			    (unsigned long long)table_addr);
			continue;
		}

		sdt_header_t *hdr = (sdt_header_t *)(uintptr_t)(u32)table_addr;
		/* ensure header mapped and checksum valid */
		if (acpi_checksum(hdr, hdr->length) == 0 &&
		    memcmp(hdr->sig, sig, 4) == 0)
			return hdr;
	}

	return NULL;
}

/* Parse FADT and attempt to enable ACPI (SCI) if possible. */
static void acpi_parse_fadt(fadt_t *fadt)
{
	if (!fadt) {
		log(LOG_ERR, "ACPI: FADT not available");
		return;
	}

	u64 dsdt_addr = (fadt->header.rev >= 3 && fadt->x_dsdt_addr)
				? fadt->x_dsdt_addr
				: fadt->dsdt_addr;
	/* u64 firmware_ctrl = (fadt->header.rev >= 3 && fadt->x_firmware_ctrl)
				    ? fadt->x_firmware_ctrl
				    : fadt->firmware_ctrl; */

	u64 pm1a_cnt_addr = 0;
	u8 addr_space = 1; /* default I/O */

	if (fadt->header.rev >= 3 && fadt->x_pm1a_cnt_blk.address) {
		pm1a_cnt_addr = fadt->x_pm1a_cnt_blk.address;
		addr_space = fadt->x_pm1a_cnt_blk.address_space_id;
	} else {
		pm1a_cnt_addr = fadt->pm1a_cnt_blk;
	}

	log(LOG_INFO, "ACPI: FADT SCI_INT=%u, SMI_CMD=0x%x, DSDT=0x%llx",
	    fadt->sci_int, fadt->smi_cmd, (unsigned long long)dsdt_addr);

	/* Validate PM1_CNT address */
	if (!pm1a_cnt_addr || (pm1a_cnt_addr > UINT32_MAX)) {
		log(LOG_WARN, "ACPI: invalid PM1_CNT address 0x%llx",
		    (unsigned long long)pm1a_cnt_addr);
		return;
	}

	u16 pm1_cnt = 0;
	if (addr_space == 1) {
		/* I/O */
		pm1_cnt = inw((u16)pm1a_cnt_addr);
	} else if (addr_space == 0) {
		/* Memory-mapped */
		if ((pm1a_cnt_addr & 1) != 0) {
			log(LOG_WARN, "ACPI: PM1_CNT not 16-bit aligned");
			return;
		}

		u32 phys = vmm_get_physical_addr((u32)pm1a_cnt_addr);
		if (!phys) {
			/* try mapping it */
			if (vmm_map_if_not_mapped((u32)pm1a_cnt_addr, PAGE_SIZE) != KERNEL_OK) {
				log(LOG_WARN, "ACPI: cannot map PM1_CNT");
				return;
			}
		}
		volatile u16 *ptr = (volatile u16 *)(uintptr_t)pm1a_cnt_addr;
		pm1_cnt = *ptr;
	} else {
		log(LOG_WARN, "ACPI: unsupported address_space_id %u", addr_space);
		return;
	}

	if (pm1_cnt & 0x1) {
		log(LOG_INFO, "ACPI: SCI_EN already set");
		return;
	}

	if (!fadt->smi_cmd) {
		log(LOG_WARN, "ACPI: no SMI_CMD to send enable");
		return;
	}

	outb(fadt->smi_cmd, fadt->acpi_enable);

	/* wait for SCI_EN */
	u32 t = TIMEOUT;
	while (t--) {
		if (addr_space == 1)
			pm1_cnt = inw((u16)pm1a_cnt_addr);
		else
			pm1_cnt = *(volatile u16 *)(uintptr_t)pm1a_cnt_addr;

		cpu_relax();
		if (pm1_cnt & 0x1) {
			log(LOG_OKAY, "ACPI: enabled (SCI_EN set)");
			return;
		}
	}

	log(LOG_ERR, "ACPI: timeout enabling SCI_EN after %u attempts", TIMEOUT);
}

/* Parse MADT and print entries we understand. */
static void acpi_parse_madt(madt_t *madt)
{
	if (!madt) {
		log(LOG_ERR, "ACPI: MADT not found");
		return;
	}

	u8 *ptr = (u8 *)madt + sizeof(sdt_header_t) + 8;
	u8 *end = (u8 *)madt + madt->header.length;
	u32 entry_count = 0;

	log(LOG_INFO, "ACPI: MADT LAPIC=0x%x flags=0x%x",
	    madt->local_apic_addr, madt->flags);

	while (ptr + 2 <= end) {
		u8 type = ptr[0];
		u8 len  = ptr[1];

		if (len < 2 || ptr + len > end) {
			log(LOG_WARN, "ACPI: invalid MADT entry at offset %zu", (size_t)(ptr - (u8 *)madt));
			break;
		}

		switch (type) {
		case 0: /* Processor Local APIC */
			if (len >= 8) {
				u8 proc = ptr[2];
				u8 apic  = ptr[3];
				u32 flags = *(u32 *)(ptr + 4);
				log(LOG_INFO, "  LAPIC: proc=%u apic=%u flags=0x%x", proc, apic, flags);
			}
			break;
		case 1: /* IO APIC */
			if (len >= 12) {
				u8 id = ptr[2];
				u32 addr = *(u32 *)(ptr + 4);
				u32 gsi = *(u32 *)(ptr + 8);
				log(LOG_INFO, "  IOAPIC: id=%u addr=0x%x gsi_base=%u", id, addr, gsi);
			}
			break;
		default:
			log(LOG_WARN, "  MADT: unknown entry type %u len %u", type, len);
			break;
		}

		ptr += len;
		entry_count++;
	}

	log(LOG_OKAY, "ACPI: MADT parse complete (%u entries)", entry_count);
}

/* Main ACPI init routine */
void acpi_init(void)
{
	u32 i;

	acpi_ctx.rsdp = acpi_find_rsdp();
	if (!acpi_ctx.rsdp) {
		log(LOG_ERR, "ACPI: RSDP not found");
		return;
	}

	log(LOG_INFO, "ACPI: RSDP at 0x%x rev=%d oem=%s",
	    (u32)acpi_ctx.rsdp, acpi_ctx.rsdp->revision, acpi_ctx.rsdp->oemid);

	bool is_xsdt = false;
	u32 sdt_phys = 0;

	if (acpi_ctx.rsdp->revision >= 2) {
		xsdp_t *xsdp = (xsdp_t *)acpi_ctx.rsdp;
		if (acpi_checksum(xsdp, xsdp->length) != 0) {
			log(LOG_ERR, "ACPI: XSDP checksum invalid");
			return;
		}
		if ((u64)xsdp->xsdt_addr > UINT32_MAX) {
			log(LOG_ERR, "ACPI: XSDT > 32-bit addr");
			return;
		}
		sdt_phys = (u32)xsdp->xsdt_addr;
		is_xsdt = true;
	} else {
		sdt_phys = acpi_ctx.rsdp->rsdt_addr;
	}

	if (vmm_map_if_not_mapped(sdt_phys, PAGE_SIZE) != KERNEL_OK) {
		log(LOG_ERR, "ACPI: failed to map SDT header");
		return;
	}

	sdt_header_t *sdt_header = (sdt_header_t *)(uintptr_t)sdt_phys;
	const char *expected = is_xsdt ? "XSDT" : "RSDT";
	if (memcmp(sdt_header->sig, expected, 4) != 0) {
		log(LOG_ERR, "ACPI: unexpected SDT signature");
		return;
	}

	u32 sdt_len = sdt_header->length;
	if (vmm_map_if_not_mapped(sdt_phys, sdt_len) != KERNEL_OK) {
		log(LOG_ERR, "ACPI: failed to map full SDT");
		return;
	}

	if (acpi_checksum(sdt_header, sdt_len) != 0) {
		log(LOG_ERR, "ACPI: SDT checksum invalid");
		return;
	}

	if (is_xsdt) {
		xsdt_t *xsdt = (xsdt_t *)sdt_header;
		acpi_ctx.sdt_ptr = xsdt->other_sdts;
		acpi_ctx.entry_size = sizeof(u64);
		acpi_ctx.num_entries = (sdt_len - sizeof(sdt_header_t)) / acpi_ctx.entry_size;
	} else {
		rsdt_t *rsdt = (rsdt_t *)sdt_header;
		acpi_ctx.sdt_ptr = rsdt->other_sdts;
		acpi_ctx.entry_size = sizeof(u32);
		acpi_ctx.num_entries = (sdt_len - sizeof(sdt_header_t)) / acpi_ctx.entry_size;
	}

	log(LOG_INFO, "ACPI: %s entries=%u", expected, acpi_ctx.num_entries);

	/* allocate small cache for table headers */
	acpi_ctx.tables = (sdt_header_t **)kcalloc(MAX_TABLES, sizeof(sdt_header_t *));
	if (!acpi_ctx.tables) {
		log(LOG_ERR, "ACPI: allocation failed for table cache");
		return;
	}

	/* iterate entries and cache up to MAX_TABLES headers */
	const u8 *entries = (const u8 *)acpi_ctx.sdt_ptr;
	u32 cached = 0;

	for (i = 0; i < acpi_ctx.num_entries && cached < MAX_TABLES; i++, entries += acpi_ctx.entry_size) {
		u64 table_phys = (acpi_ctx.entry_size == sizeof(u64))
					 ? *(const u64 *)entries
					 : *(const u32 *)entries;
		if (table_phys > UINT32_MAX) {
			log(LOG_WARN, "ACPI: skipping table at 0x%llx", (unsigned long long)table_phys);
			continue;
		}

		u32 tphys32 = (u32)table_phys;
		if (vmm_map_if_not_mapped(tphys32, PAGE_SIZE) != KERNEL_OK) {
			log(LOG_WARN, "ACPI: cannot map table header 0x%x", tphys32);
			continue;
		}

		sdt_header_t *hdr = (sdt_header_t *)(uintptr_t)tphys32;
		if (acpi_checksum(hdr, sizeof(sdt_header_t)) != 0)
			continue;

		u32 table_len = hdr->length;
		if (vmm_map_if_not_mapped(tphys32, table_len) != KERNEL_OK) {
			log(LOG_WARN, "ACPI: cannot map full table 0x%x", tphys32);
			continue;
		}

		if (acpi_checksum(hdr, table_len) == 0) {
			acpi_ctx.tables[cached++] = hdr;
			log(LOG_INFO, "ACPI: table %.4s rev=%u", hdr->sig, hdr->rev);
		}
	}

	/* FADT */
	fadt_t *fadt = (fadt_t *)acpi_get_table("FACP");
	if (fadt)
		acpi_parse_fadt(fadt);

	/* MADT */
	madt_t *madt = (madt_t *)acpi_get_table("APIC");
	if (madt) {
		log(LOG_OKAY, "ACPI: MADT present");
		acpi_parse_madt(madt);
	}
}