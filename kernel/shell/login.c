/*
 * AstraOS - Login System
 * Professional login screen with password masking
 */

#include "login.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../lib/theme.h"
#include "../drivers/pit.h"

/* Simple delay for animations */
static void delay_ms(uint32_t ms) {
    uint64_t start = pit_get_ticks();
    uint64_t target = start + (ms / 10);
    while (pit_get_ticks() < target) {
        __asm__ volatile ("hlt");
    }
}

/* Animated cursor blink */
static void print_cursor(bool visible) {
    if (visible) {
        kprintf("▊");
    } else {
        kprintf(" ");
    }
}

/* Shake animation for failed login */
static void shake_text(const char *text) {
    for (int i = 0; i < 3; i++) {
        kprintf("\r  %s", text);
        delay_ms(50);
        kprintf("\r   %s", text);
        delay_ms(50);
        kprintf("\r %s", text);
        delay_ms(50);
    }
    kprintf("\r%s", text);
}

bool login_prompt(void) {
    const ColorTheme *theme = theme_get_active();
    char username[32] = {0};
    char password[32] = {0};
    int attempts = 0;
    const int max_attempts = 3;
    
    while (attempts < max_attempts) {
        /* Clear screen */
        kprintf("\033[2J\033[H");
        
        /* Login box */
        kprintf("\n\n");
        kprintf("%s        ╔═══════════════════════════════════════╗%s\n", theme->accent1, ANSI_RESET);
        kprintf("%s        ║                                       ║%s\n", theme->accent1, ANSI_RESET);
        kprintf("%s        ║%s          %sWelcome to AstraOS%s           %s║%s\n",
                theme->accent1, ANSI_RESET, ANSI_BOLD, ANSI_RESET, theme->accent1, ANSI_RESET);
        kprintf("%s        ║                                       ║%s\n", theme->accent1, ANSI_RESET);
        kprintf("%s        ║%s     %sA Modern Operating System%s         %s║%s\n",
                theme->accent1, ANSI_RESET, theme->info, ANSI_RESET, theme->accent1, ANSI_RESET);
        kprintf("%s        ║%s         %sby Aniket%s                     %s║%s\n",
                theme->accent1, ANSI_RESET, theme->accent2, ANSI_RESET, theme->accent1, ANSI_RESET);
        kprintf("%s        ║                                       ║%s\n", theme->accent1, ANSI_RESET);
        kprintf("%s        ╚═══════════════════════════════════════╝%s\n", theme->accent1, ANSI_RESET);
        kprintf("\n\n");
        
        /* Show attempt warning if not first try */
        if (attempts > 0) {
            kprintf("%s                ⚠ Login failed! %d/%d attempts%s\n\n",
                    theme->error, attempts, max_attempts, ANSI_RESET);
        }
        
        /* Username prompt */
        kprintf("                %sUsername:%s ", theme->info, ANSI_RESET);
        
        /* Read username */
        int pos = 0;
        while (1) {
            if (!khaschar()) {
                __asm__ volatile ("hlt");
                continue;
            }
            
            char c = kgetc();
            
            if (c == '\n') {
                username[pos] = '\0';
                kprintf("\n");
                break;
            } else if (c == '\b' && pos > 0) {
                pos--;
                username[pos] = '\0';
                kprintf("\b \b");
            } else if (c >= 32 && c < 127 && pos < 31) {
                username[pos++] = c;
                kprintf("%c", c);
            }
        }
        
        /* Password prompt */
        kprintf("                %sPassword:%s ", theme->info, ANSI_RESET);
        
        /* Read password with masking */
        pos = 0;
        while (1) {
            if (!khaschar()) {
                __asm__ volatile ("hlt");
                continue;
            }
            
            char c = kgetc();
            
            if (c == '\n') {
                password[pos] = '\0';
                kprintf("\n\n");
                break;
            } else if (c == '\b' && pos > 0) {
                pos--;
                password[pos] = '\0';
                kprintf("\b \b");
            } else if (c >= 32 && c < 127 && pos < 31) {
                password[pos++] = c;
                kprintf("%s●%s", theme->accent1, ANSI_RESET);
            }
        }
        
        /* Verify credentials */
        if (strcmp(username, LOGIN_USERNAME) == 0 && strcmp(password, LOGIN_PASSWORD) == 0) {
            /* Success animation */
            kprintf("%s                ✓ Login successful!%s\n", theme->success, ANSI_RESET);
            delay_ms(1000);
            return true;
        }
        
        /* Failed login - shake animation */
        attempts++;
        if (attempts < max_attempts) {
            delay_ms(500);
        }
    }
    
    /* Max attempts reached */
    kprintf("\n%s        ✗ Maximum login attempts exceeded!%s\n", theme->error, ANSI_RESET);
    kprintf("%s        System locked. Please restart.%s\n\n", theme->error, ANSI_RESET);
    delay_ms(3000);
    
    return false;
}

void login_show_welcome(void) {
    const ColorTheme *theme = theme_get_active();
    
    kprintf("\033[2J\033[H");
    kprintf("\n\n");
    kprintf("%s        ╔═══════════════════════════════════════╗%s\n", theme->accent1, ANSI_RESET);
    kprintf("%s        ║                                       ║%s\n", theme->accent1, ANSI_RESET);
    kprintf("%s        ║%s     %s✓ Welcome back, %s!%s        %s║%s\n",
            theme->accent1, ANSI_RESET, theme->success, LOGIN_USERNAME, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s        ║                                       ║%s\n", theme->accent1, ANSI_RESET);
    kprintf("%s        ║%s   %sLoading your environment...%s       %s║%s\n",
            theme->accent1, ANSI_RESET, theme->info, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s        ║                                       ║%s\n", theme->accent1, ANSI_RESET);
    kprintf("%s        ╚═══════════════════════════════════════╝%s\n", theme->accent1, ANSI_RESET);
    kprintf("\n");
    
    delay_ms(1500);
}
