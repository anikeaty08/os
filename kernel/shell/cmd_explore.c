/*
 * AstraOS - File Explorer Command
 * Tree-style file browser
 */

#include "commands.h"
#include "../lib/stdio.h"
#include "../lib/theme.h"
#include "../lib/string.h"
#include "../fs/vfs.h"

void cmd_explore(int argc, char **argv) {
    const ColorTheme *theme = theme_get_active();
    const char *path = (argc > 1) ? argv[1] : "/";
    
    kprintf("\n");
    kprintf("%s📁 %s%s (root)%s\n", theme->info, ANSI_BOLD, path, ANSI_RESET);
    
    /* Try to read directory */
    struct vfs_node *node = vfs_open(path);
    
    if (!node) {
        kprintf("%sError:%s Directory not found: %s\n", 
                theme->error, ANSI_RESET, path);
        return;
    }
    
    if ((node->flags & 0xFF) != VFS_DIRECTORY) {
        kprintf("%sError:%s Not a directory: %s\n",
                theme->error, ANSI_RESET, path);
        vfs_close(node);
        return;
    }
    
    /* Read directory entries */
    struct dirent *entry;
    int dir_count = 0;
    int file_count = 0;
    uint64_t total_size = 0;
    uint32_t index = 0;
    
    while ((entry = vfs_readdir(node, index++)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0) {
            continue;
        }
        
        /* Get file info */
        char full_path[256];
        int path_len = strlen(path);
        int name_len = strlen(entry->name);
        
        /* Manual string concatenation to avoid snprintf */
        int i = 0;
        for (int j = 0; j < path_len && i < 255; j++) full_path[i++] = path[j];
        if (i < 255 && path[path_len-1] != '/') full_path[i++] = '/';
        for (int j = 0; j < name_len && i < 255; j++) full_path[i++] = entry->name[j];
        full_path[i] = '\0';
        
        struct vfs_node *child = vfs_open(full_path);
        
        if (child) {
            if ((child->flags & 0xFF) == VFS_DIRECTORY) {
                kprintf("├── %s📁 %s%s%s\n",
                        theme->info, theme->accent1, entry->name, ANSI_RESET);
                dir_count++;
            } else {
                uint64_t size = child->size;
                total_size += size;
                
                /* Format size */
                const char *unit = "B";
                uint64_t display_size = size;
                if (size >= 1024 * 1024) {
                    display_size = size / (1024 * 1024);
                    unit = "MB";
                } else if (size >= 1024) {
                    display_size = size / 1024;
                    unit = "KB";
                }
                
                kprintf("├── %s📄 %s%s (%llu %s)\n",
                        theme->success, entry->name, ANSI_RESET, display_size, unit);
                file_count++;
            }
            vfs_close(child);
        }
    }
    
    vfs_close(node);
    
    /* Summary */
    kprintf("\n");
    kprintf("%s%d%s directories, %s%d%s files | Total: %s%llu KB%s\n",
            theme->info, dir_count, ANSI_RESET,
            theme->info, file_count, ANSI_RESET,
            theme->accent1, total_size / 1024, ANSI_RESET);
    kprintf("\n");
}
