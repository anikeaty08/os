/*
 * AstraOS - Theme Switcher Command
 */

#include "commands.h"
#include "../lib/stdio.h"
#include "../lib/theme.h"
#include "../lib/string.h"
#include <stdbool.h>

void cmd_theme(int argc, char **argv) {
    const ColorTheme *current = theme_get_active();
    
    if (argc < 2) {
        /* Show available themes */
        kprintf("\n%sCurrent theme:%s %s%s%s\n\n", 
                ANSI_BOLD, ANSI_RESET, 
                current->accent1, current->name, ANSI_RESET);
        
        kprintf("%sAvailable themes:%s\n", ANSI_BOLD, ANSI_RESET);
        for (int i = 0; i < THEME_COUNT; i++) {
            const char *name = theme_get_name((ThemeID)i);
            if ((ThemeID)i == theme_get_current()) {
                kprintf("  %s%s%s (active)\n", current->success, name, ANSI_RESET);
            } else {
                kprintf("  %s\n", name);
            }
        }
        kprintf("\nUsage: theme <name>\n");
        kprintf("Example: theme neon\n\n");
        return;
    }
    
    /* Try to match theme name */
    const char *requested = argv[1];
    ThemeID new_theme = THEME_COUNT;
    
    for (int i = 0; i < THEME_COUNT; i++) {
        const char *name = theme_get_name((ThemeID)i);
        /* Case-insensitive comparison */
        bool match = true;
        for (int j = 0; name[j] && requested[j]; j++) {
            char c1 = name[j];
            char c2 = requested[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2 && c1 != ' ') {
                match = false;
                break;
            }
        }
        if (match) {
            new_theme = i;
            break;
        }
    }
    
    if (new_theme == THEME_COUNT) {
        kprintf("%sError:%s Unknown theme '%s'\n", 
                current->error, ANSI_RESET, requested);
        kprintf("Type 'theme' to see available themes.\n");
        return;
    }
    
    /* Set new theme */
    theme_set(new_theme);
    const ColorTheme *activated = theme_get_active();
    
    kprintf("\n%s[OK] Theme changed to:%s %s%s%s\n\n",
            activated->success, ANSI_RESET,
            activated->accent1, activated->name, ANSI_RESET);
}
