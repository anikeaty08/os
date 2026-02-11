/*
 * AstraOS - Shell Commands Implementation
 * Built-in command implementations
 */

#include "commands.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../lib/theme.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../drivers/pit.h"
#include "../arch/x86_64/cpu.h"
#include "../arch/x86_64/io.h"
#include "../drivers/acpi.h"
#include "../fs/vfs.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../drivers/serial.h"

/* External framebuffer functions */
extern void fb_clear(void);

/*
 * help - Display available commands
 */
void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const ColorTheme *theme = theme_get_active();

    kprintf("\n%s%sAstraOS Commands%s%s\n", ANSI_BOLD, theme->accent1, ANSI_RESET, ANSI_RESET);
    kprintf("%s═══════════════════════════════════════%s\n", theme->accent1, ANSI_RESET);
    
    kprintf("\n%sSystem:%s\n", theme->info, ANSI_RESET);
    kprintf("  %sstatus%s    - Live system dashboard\n", theme->accent2, ANSI_RESET);
    kprintf("  %sinfo%s      - System information\n", theme->accent2, ANSI_RESET);
    kprintf("  %smem%s       - Memory usage\n", theme->accent2, ANSI_RESET);
    kprintf("  %suptime%s    - System uptime\n", theme->accent2, ANSI_RESET);
    kprintf("  %scpuinfo%s   - CPU information\n", theme->accent2, ANSI_RESET);
    
    kprintf("\n%sFiles:%s\n", theme->info, ANSI_RESET);
    kprintf("  %sexplore%s   - Browse files (tree view)\n", theme->accent2, ANSI_RESET);
    kprintf("  %sls%s        - List directory\n", theme->accent2, ANSI_RESET);
    kprintf("  %scat%s       - Display file\n", theme->accent2, ANSI_RESET);
    
    kprintf("\n%sCustomization:%s\n", theme->info, ANSI_RESET);
    kprintf("  %stheme%s     - Change color theme\n", theme->accent2, ANSI_RESET);
    kprintf("  %sclear%s     - Clear screen\n", theme->accent2, ANSI_RESET);
    
    kprintf("\n%sUtilities:%s\n", theme->info, ANSI_RESET);
    kprintf("  %secho%s      - Print text\n", theme->accent2, ANSI_RESET);
    kprintf("  %sps%s        - List processes\n", theme->accent2, ANSI_RESET);
    kprintf("  %stest%s      - Run tests\n", theme->accent2, ANSI_RESET);
    kprintf("  %sversion%s   - Show version\n", theme->accent2, ANSI_RESET);
    kprintf("  %shelp%s      - This help\n", theme->accent2, ANSI_RESET);
    
    kprintf("\n%sPower:%s\n", theme->info, ANSI_RESET);
    kprintf("  %sreboot%s    - Restart system\n", theme->accent2, ANSI_RESET);
    kprintf("  %sshutdown%s  - Power off\n", theme->accent2, ANSI_RESET);
    
    kprintf("\n%sAbout:%s\n", theme->info, ANSI_RESET);
    kprintf("  %saniket%s    - Creator credits\n", theme->accent2, ANSI_RESET);
    kprintf("\n");
}

/*
 * clear - Clear screen
 */
void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fb_clear();
    /* ANSI escape sequence to clear serial terminal: Clear Screen and Home Cursor */
    serial_puts("\033[2J\033[H");
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
 * ls - List directory contents (READ-ONLY)
 */
void cmd_ls(int argc, char **argv) {
    const char *path = "/";

    if (argc > 1) {
        path = argv[1];
    }

    struct vfs_node *node = vfs_resolve_path(path);
    if (!node) {
        kprintf("ls: cannot access '%s': No such file or directory\n", path);
        return;
    }

    if (!vfs_is_directory(node)) {
        kprintf("ls: '%s': Not a directory\n", path);
        return;
    }

    kprintf("\nContents of %s:\n", path);
    kprintf("-------------------\n");

    struct dirent *entry;
    uint32_t index = 0;
    int count = 0;

    while ((entry = vfs_readdir(node, index++)) != NULL) {
        struct vfs_node *child = vfs_finddir(node, entry->name);
        if (child) {
            if (vfs_is_directory(child)) {
                kprintf("  [DIR]  %s\n", entry->name);
            } else {
                kprintf("  %6llu  %s\n", child->size, entry->name);
            }
            count++;
        }
    }

    if (count == 0) {
        kprintf("  (empty)\n");
    }
    kprintf("\n");
}

/*
 * cat - Display file contents (READ-ONLY)
 */
void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: cat <filename>\n");
        return;
    }

    struct vfs_node *node = vfs_open(argv[1]);
    if (!node) {
        kprintf("cat: %s: No such file or directory\n", argv[1]);
        return;
    }

    if (vfs_is_directory(node)) {
        kprintf("cat: %s: Is a directory\n", argv[1]);
        vfs_close(node);
        return;
    }

    /* Read and display file contents */
    uint64_t size = vfs_size(node);
    if (size == 0) {
        kprintf("(empty file)\n");
        vfs_close(node);
        return;
    }

    /* Limit display to 4KB for safety */
    if (size > 4096) {
        kprintf("(file too large, showing first 4096 bytes)\n");
        size = 4096;
    }

    uint8_t buffer[512];
    uint64_t offset = 0;

    kprintf("\n");
    while (offset < size) {
        uint32_t to_read = sizeof(buffer);
        if (offset + to_read > size) {
            to_read = size - offset;
        }

        int bytes = vfs_read(node, offset, to_read, buffer);
        if (bytes <= 0) break;

        /* Print as text */
        for (int i = 0; i < bytes; i++) {
            char c = buffer[i];
            if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127)) {
                kprintf("%c", c);
            } else {
                kprintf(".");
            }
        }

        offset += bytes;
    }
    kprintf("\n");

    vfs_close(node);
}

/*
 * ps - List processes
 */
void cmd_ps(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\nProcess List:\n");
    kprintf("-------------\n");
    kprintf("  PID  State      Name\n");

    /* List processes (simplified - just show count for now) */
    uint64_t count = process_count();
    kprintf("\nTotal processes: %llu\n", count);
    kprintf("Context switches: %llu\n\n", scheduler_get_switches());
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
 * shutdown - Power off the system
 */
void cmd_shutdown(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\nShutting down AstraOS...\n");

    /* Use ACPI power off */
    acpi_poweroff();

    /* If ACPI fails, fall back to halt */
    kprintf("It is now safe to turn off your computer.\n");
    cpu_halt_forever();
}

/* Helper for heap stats */
void *kmalloc(size_t size);
void kfree(void *ptr);

/*
 * aniket - Easter egg / about creator
 */
void cmd_aniket(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("\n");
    kprintf("                    ___   _   __ ______ __ __ ______ ______\n");
    kprintf("                   / _ | / | / //  _/ //_// __/_  __//_  __/\n");
    kprintf("                  / __ |/  |/ /_/ / / ,<  / _/  / /    / /   \n");
    kprintf("                 /_/ |_/_/|___/___//_/|_|/___/ /_/    /_/    \n");
    kprintf("\n");
    kprintf("       ╔═════════════════════════════════════════════════════════════╗\n");
    kprintf("       ║               🚀 ASTRAOS OPERATING SYSTEM 🚀                ║\n");
    kprintf("       ╠═════════════════════════════════════════════════════════════╣\n");
    kprintf("       ║                                                             ║\n");
    kprintf("       ║    Creator: Aniket                                          ║\n");
    kprintf("       ║    Architecture: x86_64 (Long Mode)                         ║\n");
    kprintf("       ║    Kernel Type: Monolithic Hobby Kernel                     ║\n");
    kprintf("       ║    Bootloader: Limine (Stivale2/Limine Protocol)            ║\n");
    kprintf("       ║    Language: GNU C11 with Inline Assembly                   ║\n");
    kprintf("       ║                                                             ║\n");
    kprintf("       ║    \"Building the future, one kernel at a time!\"            ║\n");
    kprintf("       ║                                                             ║\n");
    kprintf("       ╠═════════════════════════════════════════════════════════════╣\n");
    kprintf("       ║  ⭐ Kernel Subsystems:                                      ║\n");
    kprintf("       ║    • Full Physical & Virtual Memory Management              ║\n");
    kprintf("       ║    • Preemptive Multi-tasking (Ready for User-space)        ║\n");
    kprintf("       ║    • Virtual File System (VFS) with FAT16 Support           ║\n");
    kprintf("       ║    • PS/2 Keyboard & COM1 Serial I/O Abstraction           ║\n");
    kprintf("       ║    • ACPI System Control & Power Management                 ║\n");
    kprintf("       ║    • Framebuffer Graphics with Custom Font Engine           ║\n");
    kprintf("       ║                                                             ║\n");
    kprintf("       ╚═════════════════════════════════════════════════════════════╝\n");
    kprintf("\n");
}
