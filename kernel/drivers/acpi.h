/*
 * AstraOS - ACPI Driver Header
 * Basic ACPI support for power management
 */

#ifndef _ASTRA_DRIVERS_ACPI_H
#define _ASTRA_DRIVERS_ACPI_H

#include <stdint.h>
#include <stdbool.h>

/*
 * RSDP (Root System Description Pointer) structure
 */
struct acpi_rsdp {
    char signature[8];          /* "RSD PTR " */
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    /* ACPI 2.0+ fields */
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

/*
 * ACPI SDT Header (common to all tables)
 */
struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

/*
 * FADT (Fixed ACPI Description Table)
 */
struct acpi_fadt {
    struct acpi_sdt_header header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_arch_flags;
    uint8_t reserved2;
    uint32_t flags;
    /* More fields in ACPI 2.0+ but we don't need them */
} __attribute__((packed));

/*
 * Initialize ACPI subsystem
 * Returns true if ACPI is available
 */
bool acpi_init(void);

/*
 * Power off the system using ACPI
 * This function does not return on success
 */
void acpi_poweroff(void);

/*
 * Reboot the system using ACPI (if available)
 */
void acpi_reboot(void);

#endif /* _ASTRA_DRIVERS_ACPI_H */
