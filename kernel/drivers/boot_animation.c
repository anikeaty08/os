/*
 * AstraOS - Boot Animation
 * Professional boot sequence with logo and progress bars
 */

#include "boot_animation.h"
#include "../lib/stdio.h"
#include "../lib/theme.h"
#include "../drivers/pit.h"
#include "../lib/string.h"
#include "../drivers/graphics.h"

/* Simple delay */
static void delay_ms(uint32_t ms) {
    uint64_t start = pit_get_ticks();
    uint64_t target = start + (ms / 10);
    while (pit_get_ticks() < target) {
        __asm__ volatile ("hlt");
    }
}

static void print_centered(const char *text) {
    uint32_t pad = fb_center_x(text);
    for(uint32_t i=0; i<pad; i++) kprintf(" ");
    kprintf("%s\n", text);
}

void boot_animation_show(void) {
    const ColorTheme *theme = theme_get_active();
    
    /* Clear screen first */
    /* Clear screen first */
    fb_clear();
    
    /* Center the Logo */
    print_centered("      █████╗ ███████╗████████╗██████╗  █████╗     ");
    print_centered("     ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔══██╗    ");
    print_centered("     ███████║███████╗   ██║   ██████╔╝███████║    ");
    print_centered("     ██╔══██║╚════██║   ██║   ██╔══██╗██╔══██║    ");
    print_centered("     ██║  ██║███████║   ██║   ██║  ██║██║  ██║    ");
    print_centered("     ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝    ");
    kprintf("\n");
    print_centered("Operating System v0.2");
    print_centered("⭐ ═══════════════════════════════ ⭐");
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
        /* Animated progress bar */
        for (int j = 0; j <= 20; j++) {
            
            /* Clear current line first - using blank spaces for simplicity instead of just \r */
            kprintf("\r");
            
            uint32_t screen_w = fb_get_width();
            /* Total width: ~25 label + 2 brackets + 20 bar + 6 percent = 53? */
            /* Actually: 
               Label: 25 chars
               " [" : 2 chars
               Bar: 20 chars
               "] " : 2 chars
               "100%": 4 chars
               Total: 53 chars
            */
            uint32_t content_w = 55;
            uint32_t pad = (screen_w - content_w) / 2;
            
            for(uint32_t p=0; p<pad; p++) kprintf(" ");
            
            /* Print Label + Bar */
            /* Using manual padding for label to avoid %-25s issues if any */
            kprintf("%s", theme->info);
            kprintf("%s", stages[i]);
            kprintf("%s", ANSI_RESET);
            
            int label_len = strlen(stages[i]);
            for(int s=0; s<(25 - label_len); s++) kprintf(" ");
            
            kprintf(" [");
            
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
