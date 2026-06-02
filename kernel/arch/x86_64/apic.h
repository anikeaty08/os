/*
 * AstraOS - Local APIC Detection Header
 * Early APIC capability probing without interrupt routing changes
 */

#ifndef _ASTRA_ARCH_APIC_H
#define _ASTRA_ARCH_APIC_H

#include <stdbool.h>
#include <stdint.h>

#define APIC_BASE_MSR          0x1B
#define APIC_BASE_BSP          (1ULL << 8)
#define APIC_BASE_X2APIC       (1ULL << 10)
#define APIC_BASE_ENABLE       (1ULL << 11)
#define APIC_BASE_ADDR_MASK    0x000FFFFFFFFFF000ULL

#define SMP_MAX_CPUS           64

struct smp_cpu_info {
    uint32_t acpi_processor_id;
    uint32_t apic_id;
    bool enabled;
    bool online_capable;
    bool bsp;
    bool online;
};

/*
 * Read the IA32_APIC_BASE MSR.
 */
uint64_t apic_read_base_msr(void);

/*
 * Returns true when CPUID reports local APIC support.
 */
bool apic_available(void);

/*
 * Return the current processor's initial local APIC/x2APIC ID from CPUID.
 */
uint32_t apic_current_id(void);

/*
 * Probe and log local APIC state.
 *
 * This intentionally does not enable the APIC or replace PIC IRQ routing yet.
 */
void apic_init(void);

/*
 * SMP CPU topology helpers populated by ACPI MADT parsing.
 *
 * AP startup is not implemented yet; "online" currently means the BSP.
 */
void smp_topology_reset(void);
bool smp_register_lapic_cpu(uint32_t acpi_processor_id,
                            uint32_t apic_id,
                            bool enabled,
                            bool online_capable);
uint32_t smp_cpu_count(void);
uint32_t smp_enabled_cpu_count(void);
uint32_t smp_online_cpu_count(void);
const struct smp_cpu_info *smp_cpu_get(uint32_t index);
bool smp_topology_truncated(void);
bool smp_ap_startup_supported(void);
void smp_log_readiness(void);

#endif /* _ASTRA_ARCH_APIC_H */
