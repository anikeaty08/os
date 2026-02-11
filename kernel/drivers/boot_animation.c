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
    /* Clear screen */
    fb_clear();

    /* ASCII-only Logo */
    kprintf("\n\n");
    print_centered("     _    ____ _____ ____      _    ");
    print_centered("    / \\  / ___|_   _|  _ \\    / \\   ");
    print_centered("   / _ \\ \\___ \\ | | | |_) |  / _ \\  ");
    print_centered("  / ___ \\ ___) || | |  _ <  / ___ \\ ");
    print_centered(" /_/   \\_\\____/ |_| |_| \\_\\/_/   \\_\\");
    kprintf("\n");
    print_centered("Operating System v0.2");
    print_centered("================================");
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
            kprintf("\r");

            uint32_t screen_w = fb_get_width();
            uint32_t content_w = 55;
            uint32_t pad = (screen_w > content_w) ? (screen_w - content_w) / 2 : 0;

            for(uint32_t p=0; p<pad; p++) kprintf(" ");

            /* Print Label */
            kprintf("%s", stages[i]);

            int label_len = strlen(stages[i]);
            for(int s=0; s<(25 - label_len); s++) kprintf(" ");

            kprintf(" [");
            for (int k = 0; k < j; k++) kprintf("#");
            for (int k = j; k < 20; k++) kprintf("-");
            kprintf("] %3d%%", (j * 100) / 20);

            delay_ms(30);
        }
        kprintf("\n");
        delay_ms(200);
    }

    kprintf("\n");
    delay_ms(500);
}
