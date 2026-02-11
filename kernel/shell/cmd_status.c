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

    const ColorTheme *theme = theme_get_active();
    
    /* Get system information */
    uint64_t total_mem = pmm_get_total_memory();
    uint64_t used_mem = pmm_get_used_memory();
    uint64_t free_mem = pmm_get_free_memory();
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
    kprintf("%s╔═══════════════════════════════════════════════════════════╗%s\n", 
            theme->accent1, ANSI_RESET);
    kprintf("%s║%s              %sASTRAOS SYSTEM STATUS%s                  %s║%s\n",
            theme->accent1, ANSI_RESET, ANSI_BOLD, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s╠═══════════════════════════════════════════════════════════╣%s\n",
            theme->accent1, ANSI_RESET);
    kprintf("%s║%s                                                           %s║%s\n",
            theme->accent1, ANSI_RESET, theme->accent1, ANSI_RESET);
    
    /* Memory Usage Bar */
    kprintf("%s║%s  Memory:       [", theme->accent1, ANSI_RESET);
    int bar_width = 20;
    int filled = (mem_percent * bar_width) / 100;
    
    /* Color based on usage */
    const char *bar_color = theme->success;
    if (mem_percent > 70) bar_color = theme->warning;
    if (mem_percent > 90) bar_color = theme->error;
    
    kprintf("%s", bar_color);
    for (int i = 0; i < filled; i++) kprintf("█");
    kprintf("%s", ANSI_RESET);
    for (int i = filled; i < bar_width; i++) kprintf("░");
    kprintf("] %s%d%%%s (%llu/%llu MB)     %s║%s\n",
            bar_color, mem_percent, ANSI_RESET,
            used_mem / (1024 * 1024), total_mem / (1024 * 1024),
            theme->accent1, ANSI_RESET);
    
    /* Uptime */
    kprintf("%s║%s  Uptime:       %s%llu%s hours, %s%llu%s minutes, %s%llu%s seconds           %s║%s\n",
            theme->accent1, ANSI_RESET,
            theme->info, hours, ANSI_RESET,
            theme->info, minutes, ANSI_RESET,
            theme->info, seconds, ANSI_RESET,
            theme->accent1, ANSI_RESET);
    
    /* Processes (placeholder) */
    kprintf("%s║%s  Processes:    %s3%s running                                  %s║%s\n",
            theme->accent1, ANSI_RESET, theme->info, ANSI_RESET, theme->accent1, ANSI_RESET);
    
    kprintf("%s║%s                                                           %s║%s\n",
            theme->accent1, ANSI_RESET, theme->accent1, ANSI_RESET);
    
    /* Footer */
    kprintf("%s╠═══════════════════════════════════════════════════════════╣%s\n",
            theme->accent1, ANSI_RESET);
    kprintf("%s║%s  %sRecent Activity:%s                                         %s║%s\n",
            theme->accent1, ANSI_RESET, ANSI_BOLD, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s║%s    %s•%s Kernel initialized successfully                      %s║%s\n",
            theme->accent1, ANSI_RESET, theme->success, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s║%s    %s•%s Shell started                                        %s║%s\n",
            theme->accent1, ANSI_RESET, theme->success, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s║%s    %s•%s System running smoothly                             %s║%s\n",
            theme->accent1, ANSI_RESET, theme->success, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s╚═══════════════════════════════════════════════════════════╝%s\n",
            theme->accent1, ANSI_RESET);
    kprintf("\n");
}
