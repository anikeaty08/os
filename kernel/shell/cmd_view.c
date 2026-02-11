/*
 * AstraOS - View Command
 * File viewer with syntax highlighting and pagination
 */

#include "commands.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../lib/theme.h"
#include "../fs/vfs.h"

#define MAX_FILE_SIZE (1024 * 1024)  /* 1 MB max */
#define LINES_PER_PAGE 20

/* Simple syntax highlighting for common file types */
static void print_with_syntax(const char *line, const char *extension) {
    const ColorTheme *theme = theme_get_active();
    
    /* Detect file type by extension */
    bool is_code = (strcmp(extension, "c") == 0 || strcmp(extension, "h") == 0 ||
                    strcmp(extension, "cpp") == 0 || strcmp(extension, "py") == 0);
    
    if (!is_code) {
        /* Plain text - just print */
        kprintf("%s\n", line);
        return;
    }
    
    /* Simple C/C++ syntax highlighting */
    const char *keywords[] = {
        "void", "int", "char", "if", "else", "while", "for", "return",
        "struct", "typedef", "static", "const", "unsigned", "long",
        "include", "define", NULL
    };
    
    int i = 0;
    while (line[i]) {
        /* Check for keywords */
        bool found_keyword = false;
        for (int k = 0; keywords[k]; k++) {
            int len = strlen(keywords[k]);
            if (strncmp(&line[i], keywords[k], len) == 0 &&
                (i == 0 || !((line[i-1] >= 'a' && line[i-1] <= 'z') ||
                             (line[i-1] >= 'A' && line[i-1] <= 'Z')))) {
                kprintf("%s%s%s", theme->accent1, keywords[k], ANSI_RESET);
                i += len;
                found_keyword = true;
                break;
            }
        }
        
        if (found_keyword) continue;
        
        /* Check for strings */
        if (line[i] == '"') {
            kprintf("%s", theme->success);
            kprintf("%c", line[i++]);
            while (line[i] && line[i] != '"') {
                if (line[i] == '\\' && line[i+1]) {
                    kprintf("%c%c", line[i], line[i+1]);
                    i += 2;
                } else {
                    kprintf("%c", line[i++]);
                }
            }
            if (line[i] == '"') kprintf("%c", line[i++]);
            kprintf("%s", ANSI_RESET);
            continue;
        }
        
        /* Check for comments */
        if (line[i] == '/' && line[i+1] == '/') {
            kprintf("%s%s%s\n", theme->info, &line[i], ANSI_RESET);
            return;
        }
        
        /* Check for numbers */
        if (line[i] >= '0' && line[i] <= '9') {
            kprintf("%s", theme->warning);
            while (line[i] >= '0' && line[i] <= '9') {
                kprintf("%c", line[i++]);
            }
            kprintf("%s", ANSI_RESET);
            continue;
        }
        
        /* Regular character */
        kprintf("%c", line[i++]);
    }
    kprintf("\n");
}

void cmd_view(int argc, char **argv) {
    const ColorTheme *theme = theme_get_active();
    
    if (argc < 2) {
        kprintf("%sUsage:%s view <filename>\n", theme->info, ANSI_RESET);
        return;
    }
    
    const char *filename = argv[1];
    
    /* Open file */
    struct vfs_node *node = vfs_open(filename);
    if (!node) {
        kprintf("%sError:%s File not found: %s\n", theme->error, ANSI_RESET, filename);
        return;
    }
    
    if (node->type != VFS_FILE) {
        kprintf("%sError:%s Not a file: %s\n", theme->error, ANSI_RESET, filename);
        vfs_close(node);
        return;
    }
    
    /* Check file size */
    if (node->size > MAX_FILE_SIZE) {
        kprintf("%sWarning:%s File too large (max 1 MB)\n", theme->warning, ANSI_RESET);
        vfs_close(node);
        return;
    }
    
    /* Read file */
    char *buffer = (char*)kmalloc(node->size + 1);
    if (!buffer) {
        kprintf("%sError:%s Out of memory\n", theme->error, ANSI_RESET);
        vfs_close(node);
        return;
    }
    
    uint64_t bytes_read = vfs_read(node, 0, node->size, (uint8_t*)buffer);
    buffer[bytes_read] = '\0';
    vfs_close(node);
    
    /* Get file extension */
    const char *ext = strrchr(filename, '.');
    if (ext) ext++; else ext = "";
    
    /* Display header */
    kprintf("\n%s╔══════════════════════════════════════════════════════════╗%s\n",
            theme->accent1, ANSI_RESET);
    kprintf("%s║%s  File: %s%-45s%s  %s║%s\n",
            theme->accent1, ANSI_RESET, theme->accent2, filename, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s║%s  Size: %s%llu bytes%s                                        %s║%s\n",
            theme->accent1, ANSI_RESET, theme->info, bytes_read, ANSI_RESET, theme->accent1, ANSI_RESET);
    kprintf("%s╚══════════════════════════════════════════════════════════╝%s\n",
            theme->accent1, ANSI_RESET);
    kprintf("\n");
    
    /* Display content with line numbers and syntax highlighting */
    int line_num = 1;
    int lines_shown = 0;
    char line[256];
    int line_pos = 0;
    
    for (uint64_t i = 0; i <= bytes_read; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\0' || line_pos >= 255) {
            line[line_pos] = '\0';
            
            /* Print line number */
            kprintf("%s%4d%s │ ", theme->info, line_num, ANSI_RESET);
            
            /* Print line with syntax highlighting */
            print_with_syntax(line, ext);
            
            line_num++;
            lines_shown++;
            line_pos = 0;
            
            /* Pagination */
            if (lines_shown >= LINES_PER_PAGE && i < bytes_read) {
                kprintf("\n%s--- Press any key to continue, 'q' to quit ---%s",
                        theme->accent1, ANSI_RESET);
                
                while (!khaschar()) __asm__ volatile ("hlt");
                char c = kgetc();
                
                kprintf("\r%60s\r", "");  /* Clear prompt */
                
                if (c == 'q' || c == 'Q') {
                    break;
                }
                
                lines_shown = 0;
            }
            
            if (buffer[i] == '\0') break;
        } else {
            line[line_pos++] = buffer[i];
        }
    }
    
    kprintf("\n%s[End of file]%s\n\n", theme->info, ANSI_RESET);
    kfree(buffer);
}
