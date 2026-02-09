/*
 * AstraOS - Shell Commands Implementation
 * Built-in command implementations
 */

#include "commands.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../drivers/pit.h"
#include "../arch/x86_64/cpu.h"
#include "../arch/x86_64/io.h"

/* External framebuffer functions */
extern void fb_clear(void);

/*
 * help - Display available commands
 */
void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\nAvailable commands:\n");
    kprintf("-------------------\n");
    kprintf("  help      - Show this help message\n");
    kprintf("  clear     - Clear the screen\n");
    kprintf("  echo      - Print text to screen\n");
    kprintf("  mem       - Display memory information\n");
    kprintf("  uptime    - Show system uptime\n");
    kprintf("  cpuinfo   - Display CPU information\n");
    kprintf("  version   - Show AstraOS version\n");
    kprintf("  test      - Run system tests\n");
    kprintf("  reboot    - Restart the system\n");
    kprintf("  shutdown  - Halt the system\n");
    kprintf("\n");
}

/*
 * clear - Clear screen
 */
void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fb_clear();
}

/*
 * echo - Print arguments
 */
void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s", argv[i]);
        if (i < argc - 1) {
            kprintf(" ");
        }
    }
    kprintf("\n");
}

/*
 * mem - Display memory information
 */
void cmd_mem(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t total = pmm_get_total_memory();
    uint64_t free = pmm_get_free_memory();
    uint64_t used = pmm_get_used_memory();

    kprintf("\nMemory Information:\n");
    kprintf("-------------------\n");
    kprintf("  Total:  %llu MB (%llu bytes)\n", total / (1024 * 1024), total);
    kprintf("  Used:   %llu MB (%llu bytes)\n", used / (1024 * 1024), used);
    kprintf("  Free:   %llu MB (%llu bytes)\n", free / (1024 * 1024), free);
    kprintf("  Usage:  %llu%%\n", (used * 100) / total);

    kprintf("\nHeap Information:\n");
    kprintf("  Used:   %u bytes\n", (unsigned int)heap_get_used());
    kprintf("  Free:   %u bytes\n", (unsigned int)heap_get_free());
    kprintf("\n");
}

/*
 * uptime - Show system uptime
 */
void cmd_uptime(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t ticks = pit_get_ticks();
    uint64_t seconds = ticks / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;

    kprintf("\nUptime: ");
    if (hours > 0) {
        kprintf("%llu hours, ", hours);
    }
    if (minutes > 0 || hours > 0) {
        kprintf("%llu minutes, ", minutes % 60);
    }
    kprintf("%llu seconds\n", seconds % 60);
    kprintf("Total ticks: %llu\n\n", ticks);
}

/*
 * cpuinfo - Display CPU information
 */
void cmd_cpuinfo(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\nCPU Information:\n");
    kprintf("----------------\n");

    /* Get CPU vendor using CPUID */
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];

    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';

    kprintf("  Vendor: %s\n", vendor);

    /* Get CPU features */
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );

    uint32_t family = ((eax >> 8) & 0xF) + ((eax >> 20) & 0xFF);
    uint32_t model = ((eax >> 4) & 0xF) | ((eax >> 12) & 0xF0);
    uint32_t stepping = eax & 0xF;

    kprintf("  Family: %u, Model: %u, Stepping: %u\n", family, model, stepping);

    /* Feature flags */
    kprintf("  Features: ");
    if (edx & (1 << 0)) kprintf("FPU ");
    if (edx & (1 << 4)) kprintf("TSC ");
    if (edx & (1 << 5)) kprintf("MSR ");
    if (edx & (1 << 6)) kprintf("PAE ");
    if (edx & (1 << 9)) kprintf("APIC ");
    if (edx & (1 << 23)) kprintf("MMX ");
    if (edx & (1 << 25)) kprintf("SSE ");
    if (edx & (1 << 26)) kprintf("SSE2 ");
    if (ecx & (1 << 0)) kprintf("SSE3 ");
    if (ecx & (1 << 19)) kprintf("SSE4.1 ");
    if (ecx & (1 << 20)) kprintf("SSE4.2 ");
    if (ecx & (1 << 28)) kprintf("AVX ");
    kprintf("\n\n");
}

/*
 * version - Show OS version
 */
void cmd_version(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\n");
    kprintf("AstraOS v0.1\n");
    kprintf("A hobby x86_64 operating system\n");
    kprintf("Built with GCC for the Limine bootloader\n");
    kprintf("\n");
}

/*
 * test - Run system tests
 */
void cmd_test(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\nRunning system tests...\n");
    kprintf("-----------------------\n");

    /* Test memory allocation */
    kprintf("Testing kmalloc/kfree... ");
    void *ptr1 = kmalloc(1024);
    void *ptr2 = kmalloc(2048);
    void *ptr3 = kmalloc(512);

    if (ptr1 && ptr2 && ptr3) {
        memset(ptr1, 0xAA, 1024);
        memset(ptr2, 0xBB, 2048);
        memset(ptr3, 0xCC, 512);

        kfree(ptr2);
        kfree(ptr1);
        kfree(ptr3);

        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
    }

    /* Test timer */
    kprintf("Testing PIT timer... ");
    uint64_t start = pit_get_ticks();
    for (volatile int i = 0; i < 1000000; i++);
    uint64_t end = pit_get_ticks();

    if (end > start) {
        kprintf("OK (elapsed: %llu ticks)\n", end - start);
    } else {
        kprintf("FAILED\n");
    }

    /* Test string functions */
    kprintf("Testing string functions... ");
    char buf[64];
    strcpy(buf, "Hello");
    if (strcmp(buf, "Hello") == 0 && strlen(buf) == 5) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
    }

    kprintf("\nAll tests completed.\n\n");
}

/*
 * reboot - Restart the system
 */
void cmd_reboot(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\nRebooting...\n");

    /* Method 1: Keyboard controller reset */
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);

    /* Method 2: Triple fault (fallback) */
    cpu_cli();
    __asm__ volatile ("lidt (%%rax)" : : "a"(0));
    __asm__ volatile ("int $0");

    /* Should never reach here */
    cpu_halt_forever();
}

/*
 * shutdown - Halt the system
 */
void cmd_shutdown(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\nShutting down...\n");
    kprintf("It is now safe to turn off your computer.\n");

    /* Disable interrupts and halt */
    cpu_halt_forever();
}

/* Helper for heap stats */
void *kmalloc(size_t size);
void kfree(void *ptr);
