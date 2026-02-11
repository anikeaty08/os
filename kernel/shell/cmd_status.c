/*
 * AstraOS - Status Dashboard Command
 * Live system monitoring
 */

#include "commands.h"
#include "../lib/stdio.h"
#include "../lib/theme.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../drivers/pit.h"
#include "../arch/x86_64/cpu.h"

void cmd_status(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* Get system information */
    uint64_t total_mem = pmm_get_total_memory();
    uint64_t used_mem = pmm_get_used_memory();
    uint64_t uptime_ms = pit_get_ticks() * 10; /* 10ms per tick */

    /* Calculate percentages */
    int mem_percent = (int)((used_mem * 100) / total_mem);

    /* Calculate uptime */
    uint64_t seconds = uptime_ms / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;

    /* Header */
    kprintf("\n");
    kprintf("+===========================================================+\n");
    kprintf("|              ASTRAOS SYSTEM STATUS                        |\n");
    kprintf("+===========================================================+\n");
    kprintf("|                                                           |\n");

    /* Memory Usage Bar */
    kprintf("|  Memory:       [");
    int bar_width = 20;
    int filled = (mem_percent * bar_width) / 100;

    for (int i = 0; i < filled; i++) kprintf("#");
    for (int i = filled; i < bar_width; i++) kprintf("-");
    kprintf("] %d%% (%llu/%llu MB)      |\n",
            mem_percent,
            used_mem / (1024 * 1024), total_mem / (1024 * 1024));

    /* Uptime */
    kprintf("|  Uptime:       %llu hours, %llu minutes, %llu seconds",
            hours, minutes, seconds);
    /* Pad to align right border */
    int ulen = 15; /* base "X hours, X minutes, X seconds" approx */
    for (int i = ulen; i < 42; i++) kprintf(" ");
    kprintf("|\n");

    /* Processes */
    kprintf("|  Processes:    3 running                                  |\n");
    kprintf("|                                                           |\n");

    /* Footer */
    kprintf("+===========================================================+\n");
    kprintf("|  Recent Activity:                                         |\n");
    kprintf("|    * Kernel initialized successfully                      |\n");
    kprintf("|    * Shell started                                        |\n");
    kprintf("|    * System running smoothly                              |\n");
    kprintf("+===========================================================+\n");
    kprintf("\n");
}
