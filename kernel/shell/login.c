/*
 * AstraOS - Login System
 * Professional login screen with password masking
 */

#include "login.h"
#include "user.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../lib/theme.h"
#include "../drivers/pit.h"
#include "../drivers/graphics.h"

/* Simple delay for animations */
static void delay_ms(uint32_t ms) {
    uint64_t start = pit_get_ticks();
    uint64_t target = start + (ms / 10);
    while (pit_get_ticks() < target) {
        __asm__ volatile ("hlt");
    }
}

static void print_centered_text(const char *text) {
    uint32_t screen_w = fb_get_width();
    uint32_t len = strlen(text);
    uint32_t pad = (len < screen_w) ? (screen_w - len) / 2 : 0;
    for (uint32_t i = 0; i < pad; i++) kprintf(" ");
    kprintf("%s", text);
}

bool login_prompt(void) {
    char username[32] = {0};
    char password[32] = {0};
    int attempts = 0;
    const int max_attempts = 3;

    while (attempts < max_attempts) {
        /* Clear screen */
        fb_clear();

        /* Draw login box */
        kprintf("\n\n\n\n\n\n\n\n\n\n");

        print_centered_text("+------------------------------------------------+\n");
        print_centered_text("|                                                |\n");
        print_centered_text("|          Welcome to AstraOS                    |\n");
        print_centered_text("|          A Modern Operating System             |\n");
        print_centered_text("|          by Aniket                             |\n");
        print_centered_text("|                                                |\n");
        print_centered_text("+------------------------------------------------+\n");

        if (attempts > 0) {
            kprintf("\n");
            print_centered_text("  Login failed! Try again.\n");
        }

        /* Check if first user needs to be created */
        if (user_count_users() == 0) {
            kprintf("\n");
            print_centered_text("  First time setup - Create admin account\n");
            kprintf("\n");
        } else {
            kprintf("\n");
        }

        /* Username input */
        print_centered_text("  Username: ");
        int pos = 0;
        memset(username, 0, 32);
        while (1) {
            if (!khaschar()) { __asm__ volatile ("hlt"); continue; }
            char c = kgetc();
            if (c == '\n') { username[pos] = '\0'; break; }
            else if (c == '\b' && pos > 0) {
                pos--; username[pos] = '\0';
                kprintf("\b \b");
            }
            else if (c >= 32 && c < 127 && pos < 20) {
                username[pos++] = c;
                kprintf("%c", c);
            }
        }

        kprintf("\n");

        /* Password input */
        print_centered_text("  Password: ");
        pos = 0;
        memset(password, 0, 32);
        while (1) {
            if (!khaschar()) { __asm__ volatile ("hlt"); continue; }
            char c = kgetc();
            if (c == '\n') { password[pos] = '\0'; break; }
            else if (c == '\b' && pos > 0) {
                pos--; password[pos] = '\0';
                kprintf("\b \b");
            }
            else if (c >= 32 && c < 127 && pos < 20) {
                password[pos++] = c;
                kprintf("*");
            }
        }

        kprintf("\n\n");

        /* First time setup - create admin account */
        if (user_count_users() == 0) {
            if (strlen(username) > 0 && strlen(password) >= 4) {
                if (user_create(username, password, true)) {
                    if (user_authenticate(username, password)) {
                        print_centered_text("  Account created! Welcome!\n");
                        delay_ms(1000);
                        return true;
                    }
                }
            }
            print_centered_text("  Error: Username required, password min 4 chars\n");
            delay_ms(1500);
            attempts++;
            continue;
        }

        /* Normal login */
        if (user_authenticate(username, password)) {
            print_centered_text("  Login successful!\n");
            delay_ms(800);
            return true;
        }

        attempts++;
    }

    return false;
}

void login_show_welcome(void) {
    fb_clear();

    /* ASCII Logo */
    kprintf("\n\n\n\n");
    print_centered_text("     _    ____ _____ ____      _    \n");
    print_centered_text("    / \\  / ___|_   _|  _ \\    / \\   \n");
    print_centered_text("   / _ \\ \\___ \\ | | | |_) |  / _ \\  \n");
    print_centered_text("  / ___ \\ ___) || | |  _ <  / ___ \\ \n");
    print_centered_text(" /_/   \\_\\____/ |_| |_| \\_\\/_/   \\_\\\n");

    kprintf("\n\n");

    /* System info box */
    const char *current_user_name = user_get_current_name();
    if (!current_user_name) current_user_name = "Unknown";

    print_centered_text("+--------------------------------------------------------------+\n");
    print_centered_text("|              ASTRAOS OPERATING SYSTEM                        |\n");
    print_centered_text("+--------------------------------------------------------------+\n");

    kprintf("\n");
    kprintf("    User:          %s\n", current_user_name);
    kprintf("    Architecture:  x86_64 (Long Mode)\n");
    kprintf("    Kernel:        Monolithic Hobby Kernel\n");
    kprintf("    Bootloader:    Limine\n");
    kprintf("\n");

    print_centered_text("+--------------------------------------------------------------+\n");

    delay_ms(2500);
}
