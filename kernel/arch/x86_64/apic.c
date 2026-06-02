/*
 * AstraOS - Local APIC Detection Implementation
 * Groundwork for future APIC interrupt routing
 */

#include "apic.h"
#include "cpu.h"
#include "../../lib/stdio.h"
#include <stddef.h>

#define CPUID_FEATURE_LEAF  0x00000001U
#define CPUID_EDX_APIC      (1U << 9)
#define CPUID_TOPOLOGY_LEAF 0x0000000BU

static struct smp_cpu_info smp_cpus[SMP_MAX_CPUS];
static uint32_t smp_cpu_total = 0;
static bool smp_truncated = false;

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

uint32_t apic_current_id(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t max_leaf;

    cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
    if (max_leaf >= CPUID_TOPOLOGY_LEAF) {
        cpuid(CPUID_TOPOLOGY_LEAF, 0, &eax, &ebx, &ecx, &edx);
        if (ebx != 0) {
            return edx;
        }
    }

    cpuid(CPUID_FEATURE_LEAF, 0, &eax, &ebx, &ecx, &edx);
    return (ebx >> 24) & 0xFF;
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

void smp_topology_reset(void) {
    for (uint32_t i = 0; i < SMP_MAX_CPUS; i++) {
        smp_cpus[i].acpi_processor_id = 0;
        smp_cpus[i].apic_id = 0;
        smp_cpus[i].enabled = false;
        smp_cpus[i].online_capable = false;
        smp_cpus[i].bsp = false;
        smp_cpus[i].online = false;
    }

    smp_cpu_total = 0;
    smp_truncated = false;
}

bool smp_register_lapic_cpu(uint32_t acpi_processor_id,
                            uint32_t apic_id,
                            bool enabled,
                            bool online_capable) {
    uint32_t current_apic_id = apic_current_id();

    for (uint32_t i = 0; i < smp_cpu_total; i++) {
        if (smp_cpus[i].apic_id == apic_id) {
            smp_cpus[i].acpi_processor_id = acpi_processor_id;
            smp_cpus[i].enabled = enabled;
            smp_cpus[i].online_capable = online_capable;
            smp_cpus[i].bsp = (apic_id == current_apic_id);
            smp_cpus[i].online = smp_cpus[i].bsp;
            return true;
        }
    }

    if (smp_cpu_total >= SMP_MAX_CPUS) {
        smp_truncated = true;
        return false;
    }

    smp_cpus[smp_cpu_total].acpi_processor_id = acpi_processor_id;
    smp_cpus[smp_cpu_total].apic_id = apic_id;
    smp_cpus[smp_cpu_total].enabled = enabled;
    smp_cpus[smp_cpu_total].online_capable = online_capable;
    smp_cpus[smp_cpu_total].bsp = (apic_id == current_apic_id);
    smp_cpus[smp_cpu_total].online = smp_cpus[smp_cpu_total].bsp;
    smp_cpu_total++;

    return true;
}

uint32_t smp_cpu_count(void) {
    return smp_cpu_total;
}

uint32_t smp_enabled_cpu_count(void) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < smp_cpu_total; i++) {
        if (smp_cpus[i].enabled) {
            count++;
        }
    }

    return count;
}

uint32_t smp_online_cpu_count(void) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < smp_cpu_total; i++) {
        if (smp_cpus[i].online) {
            count++;
        }
    }

    return count;
}

const struct smp_cpu_info *smp_cpu_get(uint32_t index) {
    if (index >= smp_cpu_total) {
        return NULL;
    }

    return &smp_cpus[index];
}

bool smp_topology_truncated(void) {
    return smp_truncated;
}

bool smp_ap_startup_supported(void) {
    return false;
}

void smp_log_readiness(void) {
    uint32_t enabled_count = smp_enabled_cpu_count();
    uint32_t online_count = smp_online_cpu_count();

    kprintf("SMP: topology has %u CPU entry(s), %u enabled, %u online\n",
            smp_cpu_total,
            enabled_count,
            online_count);

    for (uint32_t i = 0; i < smp_cpu_total; i++) {
        const struct smp_cpu_info *cpu = &smp_cpus[i];
        kprintf("SMP: CPU%u ACPI ID=%u LAPIC ID=%u enabled=%s online-capable=%s BSP=%s online=%s\n",
                i,
                cpu->acpi_processor_id,
                cpu->apic_id,
                cpu->enabled ? "yes" : "no",
                cpu->online_capable ? "yes" : "no",
                cpu->bsp ? "yes" : "no",
                cpu->online ? "yes" : "no");
    }

    if (smp_truncated) {
        kprintf("SMP: topology truncated at %u CPU entries\n", SMP_MAX_CPUS);
    }

    if (smp_cpu_total == 0) {
        kprintf("SMP: blocked: no ACPI MADT LAPIC CPU entries discovered\n");
        return;
    }

    if (!apic_available()) {
        kprintf("SMP: blocked: CPUID local APIC feature not present\n");
        return;
    }

    if (enabled_count <= 1) {
        kprintf("SMP: BSP-only topology ready; no enabled APs to start\n");
        return;
    }

    if (!smp_ap_startup_supported()) {
        kprintf("SMP: AP startup blocked: trampoline, INIT/SIPI IPI delivery, and per-CPU bootstrap are not implemented\n");
        return;
    }

    kprintf("SMP: AP startup supported\n");
}
