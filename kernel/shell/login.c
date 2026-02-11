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

/* Simple delay for animations */
#include "../drivers/graphics.h"

static void draw_centered_box(uint32_t width, uint32_t height, const ColorTheme *theme) {
    uint32_t start_x = fb_center_x("") - (width / 2);
    uint32_t start_y = fb_center_y(height) - (height / 2);
    
    /* Move to top-left of box */
    for(uint32_t y=0; y<start_y; y++) kprintf("\n");
    
    /* Top Border: ╔ + (width-2) ═ + ╗ */
    for(uint32_t x=0; x<start_x; x++) kprintf(" ");
    kprintf("%s╔", theme->accent1);
    for(uint32_t i=0; i<width-2; i++) kprintf("═");
    kprintf("╗%s\n", ANSI_RESET);
    
    /* Middle Rows: ║ + (width-2) spaces + ║ */
    for(uint32_t h=0; h<height-2; h++) {
        for(uint32_t x=0; x<start_x; x++) kprintf(" ");
        kprintf("%s║", theme->accent1);
        for(uint32_t i=0; i<width-2; i++) kprintf(" ");
        kprintf("║%s\n", ANSI_RESET);
    }
    
    /* Bottom Border: ╚ + (width-2) ═ + ╝ */
    for(uint32_t x=0; x<start_x; x++) kprintf(" ");
    kprintf("%s╚", theme->accent1);
    for(uint32_t i=0; i<width-2; i++) kprintf("═");
    kprintf("╝%s\n", ANSI_RESET);
}

/* Helper to place cursor at specific (x,y) relative to box top-left */
static void set_cursor_in_box(uint32_t box_w, uint32_t box_h, uint32_t rel_x, uint32_t rel_y) {
    uint32_t start_x = fb_center_x("") - (box_w / 2);
    /* Note: Vertical positioning with standard terminal newlines is tricky.
       We will rely on rewriting the whole frame for simplicity in this robust-safe mode,
       OR we assume we are drawing top-down. 
       
       For a truly interactive experience without ncurses, re-drawing the whole screen 
       on each character input is safest to guarantee alignment, although flickering.
       
       Better approach: Use ANSI cursor positioning \033[line;columnH
    */
    
    uint32_t abs_y = (fb_get_height() / 2) - (box_h / 2) + rel_y + 1; // +1 for 1-based index
    uint32_t abs_x = start_x + rel_x + 1;
    
    kprintf("\033[%d;%dH", abs_y, abs_x);
}

bool login_prompt(void) {
    const ColorTheme *theme = theme_get_active();
    char username[32] = {0};
    char password[32] = {0};
    int attempts = 0;
    const int max_attempts = 3;
    
    uint32_t box_w = 50;
    uint32_t box_h = 14; /* Increased height */
    
    while (attempts < max_attempts) {
        /* Clear screen */
        kprintf("\033[2J\033[H");
        
        /* Draw the static layout */
        draw_centered_box(box_w, box_h, theme);
        
        /* Title */
        const char *title = "Welcome to AstraOS";
        set_cursor_in_box(box_w, box_h, (box_w - strlen(title))/2, 2);
        kprintf("%s%s%s%s", ANSI_BOLD, theme->accent1, title, ANSI_RESET);
        
        const char *sub = "A Modern Operating System";
        set_cursor_in_box(box_w, box_h, (box_w - strlen(sub))/2, 4);
        kprintf("%s%s%s%s", theme->accent2, sub, ANSI_RESET);
        
        const char *author = "by Aniket";
        set_cursor_in_box(box_w, box_h, (box_w - strlen(author))/2, 5);
        kprintf("%s%s%s", theme->info, author, ANSI_RESET);
        
        /* Error Message Position */
        if (attempts > 0) {
            char err[64];
            snprintf(err, 64, "Login failed! %d/%d attempts", attempts, max_attempts); // Using safe snprintf
            set_cursor_in_box(box_w, box_h, (box_w - strlen(err))/2, 11);
            kprintf("%s%s%s", theme->error, err, ANSI_RESET);
        }

        /* 
         * Username Input 
         */
        const char *user_lbl = "Username: ";
        set_cursor_in_box(box_w, box_h, 8, 8); // Left aligned, indented 8 chars
        kprintf("%s%s%s%s", theme->info, user_lbl, ANSI_RESET, username);
        
        /* Read Username loop */
        if (strlen(username) == 0) { // Only read if empty (allows re-drawing logic if we wanted)
            int pos = 0;
            while(1) {
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
        }
        
        /* 
         * Password Input 
         */
        const char *pass_lbl = "Password: ";
        set_cursor_in_box(box_w, box_h, 8, 9); // Left aligned, below username
        kprintf("%s%s%s", theme->info, pass_lbl, ANSI_RESET);
        
        int pos = 0;
        memset(password, 0, 32);
        while(1) {
            if (!khaschar()) { __asm__ volatile ("hlt"); continue; }
            char c = kgetc();
            if (c == '\n') { password[pos] = '\0'; break; }
            else if (c == '\b' && pos > 0) { 
                pos--; password[pos] = '\0'; 
                kprintf("\b \b"); 
            }
            else if (c >= 32 && c < 127 && pos < 20) { 
                password[pos++] = c; 
                kprintf("%s●%s", theme->accent1, ANSI_RESET); 
            }
        }
        
        /* Verify */
        if (user_authenticate(username, password)) {
            set_cursor_in_box(box_w, box_h, (box_w - 9)/2, 11); // Center "Success"
            kprintf("%s✓ Success%s", theme->success, ANSI_RESET);
            delay_ms(800);
            return true;
        }
        
        attempts++;
        memset(username, 0, 32); // Reset for retry
    }
    
    return false;
}

void login_show_welcome(void) {
    const ColorTheme *theme = theme_get_active();
    kprintf("\033[2J\033[H");
    
    /* Large ASCII Logo */
    const char *logo[] = {
        "                    ___   _   __ ______ __ __ ______ ______        ",
        "                   / _ | / | / //  _/ //_// __/_  __//_  __/       ",
        "                  / __ |/  |/ /_/ / / ,<  / _/  / /    / /         ",
        "                 /_/ |_/_/|___/___//_/|_|/___/ /_/    /_/          ",
        NULL
    };
    
    uint32_t start_y = fb_center_y(18); // Approx total height
    for(uint32_t i=0; i<start_y; i++) kprintf("\n");
    
    for(int i=0; logo[i]; i++) {
         uint32_t pad = fb_center_x(logo[i]);
         for(uint32_t p=0; p<pad; p++) kprintf(" ");
         kprintf("%s%s%s\n", theme->accent1, logo[i], ANSI_RESET);
    }
    
    kprintf("\n");
    
    uint32_t box_w = 64; 
    uint32_t start_x = fb_center_x("") - (box_w/2);
    
    /* Draw System Info Box Header */
    /* Top Border */
    for(uint32_t x=0; x<start_x; x++) kprintf(" ");
    kprintf("%s╔", theme->info);
    for(uint32_t i=0; i<box_w-2; i++) kprintf("═");
    kprintf("╗%s\n", ANSI_RESET);
    
    const char *title = "🚀 ASTRAOS OPERATING SYSTEM 🚀";
    /* Title Row with proper padding */
    /* Total inner width = box_w - 2 */
    /* Title len needs to be centered in inner width */
    uint32_t inner_w = box_w - 2;
    uint32_t title_len = 30; /* Manual length count for "🚀 ASTRAOS OPERATING SYSTEM 🚀" roughly */
                             /* Actually standard ASCII length is simpler: just the text */
                             /* But wait, rocket emoji is multibyte. Let's simplify title padding manually */
    
    for(uint32_t x=0; x<start_x; x++) kprintf(" ");
    kprintf("%s║%s", theme->info, ANSI_RESET);
    
    /* Center text manually for 64 width box */
    /* "🚀 ASTRAOS OPERATING SYSTEM 🚀" has visual width approx 32 chars */
    /* (62 - 32) / 2 = 15 spaces padding */
    for(int i=0; i<15; i++) kprintf(" ");
    kprintf("%s%s%s", ANSI_BOLD, title, ANSI_RESET);
    
    /* Pad remaining space (62 - 15 - ~32 = 15) */
    /* Due to emoji width variation, best to pad strictly based on visual check or simplify */
    /* Let's just fill broadly to fit */
    for(int i=0; i<15; i++) kprintf(" ");
    kprintf("%s║%s\n", theme->info, ANSI_RESET);
    
    /* Separator */
    for(uint32_t x=0; x<start_x; x++) kprintf(" ");
    kprintf("%s╠", theme->info);
    for(uint32_t i=0; i<box_w-2; i++) kprintf("═");
    kprintf("╣%s\n", ANSI_RESET);
    
    /* Info Lines: Box width 64 -> inner 62 chars */
    char user_line[64];
    /* Using safe snprintf to avoid buffer overflows if username is long */
    /* Since we don't have standard snprintf, we construct carefully or assume short username */
    /* But wait, we have extended ASCII box drawing... let's keep it simple */
    
    /* We can't initialize VLA in this context easily in kernel C without standard libs sometimes */
    const char *current_user_name = user_get_current_name();
    
    /* Manually constructing the lines array to include dynamic content */
    /* We'll just print them one by one instead of an array */
    
    const char *labels[] = {
        "User:", 
        "Architecture:",
        "Kernel:",
        "Bootloader:",
        NULL
    };
    
    const char *values[] = {
        current_user_name,
        "x86_64 (Long Mode)",
        "Monolithic Hobby Kernel",
        "Limine",
        NULL
    };
    
    for(int i=0; labels[i]; i++) {
        for(uint32_t x=0; x<start_x; x++) kprintf(" ");
        kprintf("%s║%s    %s %s", theme->info, theme->accent2, labels[i], values[i]);
        
        /* Calculate padding dynamically */
        int content_len = 4 + strlen(labels[i]) + 1 + strlen(values[i]); // "    Label: Value"
        int pad = inner_w - content_len;
        for(int p=0; p<pad; p++) kprintf(" ");
        
        kprintf("%s║%s\n", theme->info, ANSI_RESET);
    }
    
    /* Bottom Border */
    for(uint32_t x=0; x<start_x; x++) kprintf(" ");
    kprintf("%s╚", theme->info);
    for(uint32_t i=0; i<box_w-2; i++) kprintf("═");
    kprintf("╝%s\n", ANSI_RESET);
    
    delay_ms(2500);
}
