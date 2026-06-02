/*
 * AstraOS - Local APIC Detection Implementation
 * Groundwork for future APIC interrupt routing
 */

#include "apic.h"
#include "cpu.h"
#include "../../lib/stdio.h"

#define CPUID_FEATURE_LEAF  0x00000001U
#define CPUID_EDX_APIC      (1U << 9)

static void cpuid(uint32_t leaf,
                  uint32_t subleaf,
                  uint32_t *eax,
                  uint32_t *ebx,
                  uint32_t *ecx,
                  uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

uint64_t apic_read_base_msr(void) {
    return cpu_rdmsr(APIC_BASE_MSR);
}

bool apic_available(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    cpuid(CPUID_FEATURE_LEAF, 0, &eax, &ebx, &ecx, &edx);
    return (edx & CPUID_EDX_APIC) != 0;
}

void apic_init(void) {
    kprintf("APIC: probing local APIC support...\n");

    if (!apic_available()) {
        kprintf("APIC: CPUID local APIC feature not present; keeping PIC routing\n");
        return;
    }

    uint64_t apic_base = apic_read_base_msr();
    uint64_t mmio_base = apic_base & APIC_BASE_ADDR_MASK;

    kprintf("APIC: CPUID local APIC feature present\n");
    kprintf("APIC: IA32_APIC_BASE=0x%llx, MMIO base=0x%llx\n",
            apic_base,
            mmio_base);
    kprintf("APIC: BSP=%s, x2APIC=%s, enabled=%s\n",
            (apic_base & APIC_BASE_BSP) ? "yes" : "no",
            (apic_base & APIC_BASE_X2APIC) ? "yes" : "no",
            (apic_base & APIC_BASE_ENABLE) ? "yes" : "no");
    kprintf("APIC: detection only; keeping PIC routing\n");
}
