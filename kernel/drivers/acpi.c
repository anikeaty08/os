/*
 * AstraOS - ACPI Driver Implementation
 * Basic ACPI support for power management
 */

#include "acpi.h"
#include "../arch/x86_64/io.h"
#include "../arch/x86_64/cpu.h"
#include "../lib/string.h"
#include "../lib/stdio.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"

/*
 * ACPI state
 */
static bool acpi_available = false;
static struct acpi_fadt *fadt = NULL;
static uint16_t slp_typa = 0;
static uint16_t slp_typb = 0;
static uint16_t pm1a_cnt = 0;
static uint16_t pm1b_cnt = 0;

/*
 * Local HHDM offset for ACPI subsystem
 */
static uint64_t acpi_hhdm_offset = 0;

/*
 * Convert physical address to virtual using HHDM
 * Also ensures the page is mapped in the kernel page tables
 */
static void *phys_to_virt(uint64_t phys) {
    uint64_t virt = phys + acpi_hhdm_offset;

    /* Check if this page is already mapped */
    if (vmm_virt_to_phys(NULL, virt & ~0xFFFULL) == 0) {
        /* Page not mapped - map it into the HHDM */
        vmm_map_page(NULL, virt & ~0xFFFULL, phys & ~0xFFFULL, PTE_WRITABLE);
    }

    return (void *)virt;
}

/*
 * Ensure a range of physical memory is mapped via HHDM
 */
static void acpi_map_range(uint64_t phys_start, size_t length) {
    uint64_t start_page = phys_start & ~0xFFFULL;
    uint64_t end = phys_start + length;

    for (uint64_t page = start_page; page < end; page += PAGE_SIZE) {
        uint64_t virt = page + acpi_hhdm_offset;
        if (vmm_virt_to_phys(NULL, virt) == 0) {
            vmm_map_page(NULL, virt, page, PTE_WRITABLE);
        }
    }
}

/*
 * Validate ACPI table checksum
 */
static bool acpi_checksum_valid(void *table, size_t length) {
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t *)table;

    for (size_t i = 0; i < length; i++) {
        sum += ptr[i];
    }

    return sum == 0;
}

/*
 * Find RSDP in memory (via HHDM-mapped BIOS ROM area)
 * Only called as fallback if bootloader doesn't provide RSDP
 */
static struct acpi_rsdp *acpi_find_rsdp(void) {
    /* Safety check: we need a valid HHDM offset to scan BIOS ROM */
    if (acpi_hhdm_offset == 0) {
        kprintf("ACPI: Cannot scan BIOS ROM without HHDM offset\n");
        return NULL;
    }

    /* Search BIOS area */
    for (uint64_t addr = 0x000E0000; addr < 0x00100000; addr += 16) {
        struct acpi_rsdp *rsdp = phys_to_virt(addr);

        if (memcmp(rsdp->signature, "RSD PTR ", 8) == 0) {
            /* Validate checksum */
            if (acpi_checksum_valid(rsdp, 20)) {
                return rsdp;
            }
        }
    }

    return NULL;
}

/*
 * Find ACPI table by signature
 */
static void *acpi_find_table(struct acpi_rsdp *rsdp, const char *signature) {
    if (!rsdp) return NULL;

    /* Use XSDT if ACPI 2.0+, otherwise RSDT */
    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        /* Map the XSDT header first to read its length */
        struct acpi_sdt_header *xsdt = phys_to_virt(rsdp->xsdt_address);

        if (memcmp(xsdt->signature, "XSDT", 4) != 0) {
            return NULL;
        }

        /* Map the full XSDT */
        acpi_map_range(rsdp->xsdt_address, xsdt->length);

        int entries = (xsdt->length - sizeof(struct acpi_sdt_header)) / 8;
        uint64_t *table_ptrs = (uint64_t *)((uint8_t *)xsdt + sizeof(struct acpi_sdt_header));

        for (int i = 0; i < entries; i++) {
            struct acpi_sdt_header *header = phys_to_virt(table_ptrs[i]);
            if (memcmp(header->signature, signature, 4) == 0) {
                /* Map the full table */
                acpi_map_range(table_ptrs[i], header->length);
                return header;
            }
        }
    } else {
        /* Map the RSDT header first */
        struct acpi_sdt_header *rsdt = phys_to_virt(rsdp->rsdt_address);

        if (memcmp(rsdt->signature, "RSDT", 4) != 0) {
            return NULL;
        }

        /* Map the full RSDT */
        acpi_map_range(rsdp->rsdt_address, rsdt->length);

        int entries = (rsdt->length - sizeof(struct acpi_sdt_header)) / 4;
        uint32_t *table_ptrs = (uint32_t *)((uint8_t *)rsdt + sizeof(struct acpi_sdt_header));

        for (int i = 0; i < entries; i++) {
            struct acpi_sdt_header *header = phys_to_virt((uint64_t)table_ptrs[i]);
            if (memcmp(header->signature, signature, 4) == 0) {
                /* Map the full table */
                acpi_map_range((uint64_t)table_ptrs[i], header->length);
                return header;
            }
        }
    }

    return NULL;
}

/*
 * Parse DSDT to find S5 sleep type values
 * This is a simplified parser - real ACPI requires AML interpretation
 */
static bool acpi_parse_s5(struct acpi_fadt *fadt_ptr) {
    if (!fadt_ptr || !fadt_ptr->dsdt) return false;

    /* Map DSDT header first to read its length */
    struct acpi_sdt_header *dsdt = phys_to_virt((uint64_t)fadt_ptr->dsdt);

    if (memcmp(dsdt->signature, "DSDT", 4) != 0) {
        return false;
    }

    /* Map the full DSDT */
    acpi_map_range((uint64_t)fadt_ptr->dsdt, dsdt->length);

    /* Search for "_S5_" in DSDT */
    uint8_t *ptr = (uint8_t *)dsdt;
    uint8_t *end = ptr + dsdt->length;

    while (ptr < end - 4) {
        if (memcmp(ptr, "_S5_", 4) == 0) {
            /* Found S5 object, parse the package */
            /* Skip past "_S5_" and look for package data */
            ptr += 4;

            /* Skip potential name path prefix */
            if (*ptr == 0x08) ptr++;  /* NameOp */

            /* Look for Package op (0x12) */
            while (ptr < end && *ptr != 0x12) ptr++;
            if (ptr >= end) break;

            ptr++;  /* Skip Package op */
            ptr++;  /* Skip package length */
            ptr++;  /* Skip element count */

            /* Get SLP_TYPa value */
            if (*ptr == 0x0A) {  /* BytePrefix */
                ptr++;
                slp_typa = *ptr;
                ptr++;
            } else if (*ptr == 0x0B) {  /* WordPrefix */
                ptr++;
                slp_typa = *(uint16_t *)ptr;
                ptr += 2;
            } else {
                slp_typa = *ptr++;
            }

            /* Get SLP_TYPb value (might be same or different) */
            if (*ptr == 0x0A) {
                ptr++;
                slp_typb = *ptr;
            } else if (*ptr == 0x0B) {
                ptr++;
                slp_typb = *(uint16_t *)ptr;
            } else {
                slp_typb = *ptr;
            }

            return true;
        }
        ptr++;
    }

    /* Fallback: common values for QEMU/Bochs */
    slp_typa = 5;
    slp_typb = 5;
    return true;
}

/*
 * Initialize ACPI subsystem
 * @param rsdp_ptr: Pointer to RSDP from bootloader (already HHDM-mapped), or NULL
 * @param hhdm: HHDM offset for physical-to-virtual conversion
 */
bool acpi_init(void *rsdp_ptr, uint64_t hhdm) {
    struct acpi_rsdp *rsdp;

    /* Store HHDM offset for phys_to_virt conversions */
    acpi_hhdm_offset = hhdm;

    /* Map BIOS ROM area (0xE0000-0x100000) where RSDP typically lives */
    acpi_map_range(0x000E0000, 0x00020000);

    /* Use provided RSDP from bootloader if available */
    if (rsdp_ptr) {
        rsdp = (struct acpi_rsdp *)rsdp_ptr;
    } else {
        /* Fallback: search for RSDP in BIOS ROM via HHDM */
        rsdp = acpi_find_rsdp();
    }

    if (!rsdp) {
        kprintf("ACPI: RSDP not found\n");
        return false;
    }

    kprintf("ACPI: Found RSDP (revision %d)\n", rsdp->revision);

    /* Find FADT */
    fadt = acpi_find_table(rsdp, "FACP");
    if (!fadt) {
        kprintf("ACPI: FADT not found\n");
        return false;
    }

    /* Get PM1a/PM1b control block addresses */
    pm1a_cnt = fadt->pm1a_control_block;
    pm1b_cnt = fadt->pm1b_control_block;

    kprintf("ACPI: PM1a_CNT=0x%x, PM1b_CNT=0x%x\n", pm1a_cnt, pm1b_cnt);

    /* Parse DSDT for S5 sleep type */
    if (!acpi_parse_s5(fadt)) {
        kprintf("ACPI: Could not parse S5 sleep type (using defaults)\n");
        slp_typa = 5;
        slp_typb = 5;
    }

    acpi_available = true;
    return true;
}

/*
 * Power off the system using ACPI
 */
void acpi_poweroff(void) {
    kprintf("\nACPI: Initiating system shutdown...\n");

    /* Disable interrupts */
    cpu_cli();

    /* Try ACPI shutdown if available */
    if (acpi_available && pm1a_cnt) {
        /* SLP_EN is bit 13, SLP_TYP is bits 10-12 */
        uint16_t val = (slp_typa << 10) | (1 << 13);

        outw(pm1a_cnt, val);

        if (pm1b_cnt) {
            val = (slp_typb << 10) | (1 << 13);
            outw(pm1b_cnt, val);
        }
    }

    /* Fallback methods for various emulators */

    /* QEMU ACPI shutdown (newer) */
    outw(0x604, 0x2000);

    /* Bochs/older QEMU shutdown */
    outw(0xB004, 0x2000);

    /* VirtualBox ACPI shutdown */
    outw(0x4004, 0x3400);

    /* Cloud Hypervisor */
    outw(0x600, 0x34);

    /* If nothing worked, halt */
    kprintf("ACPI: Shutdown failed, halting CPU\n");
    kprintf("It is now safe to turn off your computer.\n");

    for (;;) {
        cpu_hlt();
    }
}

/*
 * Reboot the system
 */
void acpi_reboot(void) {
    kprintf("\nACPI: Initiating system reboot...\n");

    cpu_cli();

    /* Try keyboard controller reset first */
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);

    /* Try ACPI reset if available */
    if (acpi_available && fadt) {
        /* Check if reset register is valid (ACPI 2.0+) */
        /* For simplicity, we'll use the keyboard controller method above */
    }

    /* Triple fault as last resort */
    __asm__ volatile ("lidt (%%rax)" : : "a"(0));
    __asm__ volatile ("int $0");

    /* Should never reach here */
    for (;;) {
        cpu_hlt();
    }
}
