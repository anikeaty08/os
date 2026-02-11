/*
 * AstraOS - Boot Animation
 * Professional boot sequence with logo and progress bars
 */

#include "boot_animation.h"
#include "../lib/stdio.h"
#include "../lib/theme.h"
#include "../drivers/pit.h"

/* Simple delay function */
static void delay_ms(uint32_t ms) {
    uint64_t start = pit_get_ticks();
    uint64_t target = start + (ms / 10); /* 10ms per tick */
    while (pit_get_ticks() < target) {
        __asm__ volatile ("hlt");
    }
}

void boot_animation_show(void) {
    const ColorTheme *theme = theme_get_active();
    
    /* Clear screen first */
    kprintf("\033[2J\033[H"); /* ANSI clear screen */
    
    /* Logo */
    kprintf("\n\n");
    kprintf("%s        ⭐ ═══════════════════════════════ ⭐%s\n", theme->accent1, ANSI_RESET);
    kprintf("%s           %s\n", theme->accent1, ANSI_RESET);
    kprintf("%s              █████╗ ███████╗████████╗██████╗  █████╗ %s\n", theme->accent2, ANSI_RESET);
    kprintf("%s             ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔══██╗%s\n", theme->accent2, ANSI_RESET);
    kprintf("%s             ███████║███████╗   ██║   ██████╔╝███████║%s\n", theme->accent2, ANSI_RESET);
    kprintf("%s             ██╔══██║╚════██║   ██║   ██╔══██╗██╔══██║%s\n", theme->accent2, ANSI_RESET);
    kprintf("%s             ██║  ██║███████║   ██║   ██║  ██║██║  ██║%s\n", theme->accent2, ANSI_RESET);
    kprintf("%s             ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝%s\n", theme->accent2, ANSI_RESET);
    kprintf("%s                                                    %s\n", theme->accent1, ANSI_RESET);
    kprintf("%s                    %sOperating System v0.2%s%s\n", 
            theme->accent1, ANSI_BOLD, ANSI_RESET, ANSI_RESET);
    kprintf("%s        ⭐ ═══════════════════════════════ ⭐%s\n", theme->accent1, ANSI_RESET);
    kprintf("\n\n");
    
    delay_ms(1000);
    
    /* Progress bars */
    const char *stages[] = {
        "Initializing Kernel",
        "Loading Memory Manager",
        "Starting Drivers",
        "Mounting Filesystem",
        "Starting Shell"
    };
    
    for (int i = 0; i < 5; i++) {
        kprintf("    %s%-25s%s [", theme->info, stages[i], ANSI_RESET);
        
        /* Animated progress bar */
        for (int j = 0; j <= 20; j++) {
            if (j > 0) {
                kprintf("\r    %s%-25s%s [", theme->info, stages[i], ANSI_RESET);
            }
            
            kprintf("%s", theme->success);
            for (int k = 0; k < j; k++) kprintf("█");
            kprintf("%s", ANSI_RESET);
            for (int k = j; k < 20; k++) kprintf("░");
            kprintf("] %s%d%%%s", theme->success, (j * 100) / 20, ANSI_RESET);
            
            delay_ms(30);
        }
        kprintf("\n");
        delay_ms(200);
    }
    
    kprintf("\n");
    delay_ms(500);
}
