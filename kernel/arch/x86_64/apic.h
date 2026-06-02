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

/*
 * Read the IA32_APIC_BASE MSR.
 */
uint64_t apic_read_base_msr(void);

/*
 * Returns true when CPUID reports local APIC support.
 */
bool apic_available(void);

/*
 * Probe and log local APIC state.
 *
 * This intentionally does not enable the APIC or replace PIC IRQ routing yet.
 */
void apic_init(void);

#endif /* _ASTRA_ARCH_APIC_H */
