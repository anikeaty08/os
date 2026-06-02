/*
 * AstraOS - Virtual File System Implementation
 * Abstract filesystem interface (READ-ONLY)
 *
 * This VFS supports read and bounded write operations when the mounted
 * filesystem exposes them.
 */

#include "vfs.h"
#include "../lib/string.h"
#include "../mm/heap.h"

/*
 * Root filesystem node
 */
static struct vfs_node *vfs_root = NULL;

/*
 * Initialize VFS
 */
void vfs_init(void) {
    vfs_root = NULL;
}

/*
 * Mount root filesystem
 */
int vfs_mount_root(struct vfs_node *root) {
    if (!root) return -1;
    vfs_root = root;
    return 0;
}

/*
 * Get root node
 */
struct vfs_node *vfs_get_root(void) {
    return vfs_root;
}

/*
 * Read from file
 */
int vfs_read(struct vfs_node *node, uint64_t offset, size_t size, uint8_t *buffer) {
    if (!node || !buffer) return -1;
    if (!vfs_can_read(node)) return -1;

    if (node->read) {
        return node->read(node, offset, size, buffer);
    }

    return -1;
}

/*
 * Write to file
 */
int vfs_write(struct vfs_node *node, uint64_t offset, size_t size, const uint8_t *buffer) {
    if (!node || !buffer) return -1;
    if (!vfs_can_write(node)) return -1;

    if (node->write) {
        return node->write(node, offset, size, buffer);
    }

    return -1;
}

/*
 * Read directory entry
 */
struct dirent *vfs_readdir(struct vfs_node *node, uint32_t index) {
    if (!node) return NULL;
    if (!vfs_can_read(node)) return NULL;

    /* Check if it's a directory */
    if (!(node->flags & VFS_DIRECTORY)) {
        return NULL;
    }

    if (node->readdir) {
        return node->readdir(node, index);
    }

    return NULL;
}

/*
 * Find entry in directory
 */
struct vfs_node *vfs_finddir(struct vfs_node *node, const char *name) {
    if (!node || !name) return NULL;
    if (!vfs_can_exec(node)) return NULL;

    /* Check if it's a directory */
    if (!(node->flags & VFS_DIRECTORY)) {
        return NULL;
    }

    if (node->finddir) {
        return node->finddir(node, name);
    }

    return NULL;
}

/*
 * Open file (call open callback if exists)
 */
struct vfs_node *vfs_open(const char *path) {
    struct vfs_node *node = vfs_resolve_path(path);

    if (node && !vfs_can_read(node)) {
        return NULL;
    }

    if (node && node->open) {
        if (node->open(node) != 0) {
            return NULL;
        }
    }

    return node;
}

/*
 * Check read permission
 */
bool vfs_can_read(struct vfs_node *node) {
    if (!node) return false;
    return (node->permissions & VFS_PERM_READ) != 0;
}

/*
 * Check execute/traverse permission
 */
bool vfs_can_exec(struct vfs_node *node) {
    if (!node) return false;
    return (node->permissions & VFS_PERM_EXEC) != 0;
}

/*
 * Check write permission
 */
bool vfs_can_write(struct vfs_node *node) {
    if (!node) return false;
    return (node->permissions & VFS_PERM_WRITE) != 0;
}

bool vfs_can_read_as(struct vfs_node *node, uint32_t uid, bool is_admin) {
    (void)uid;
    return is_admin || vfs_can_read(node);
}

bool vfs_can_write_as(struct vfs_node *node, uint32_t uid, bool is_admin) {
    if (!node) return false;
    return is_admin || (node->uid == uid && vfs_can_write(node));
}

bool vfs_can_exec_as(struct vfs_node *node, uint32_t uid, bool is_admin) {
    (void)uid;
    return is_admin || vfs_can_exec(node);
}

/*
 * Close file
 */
void vfs_close(struct vfs_node *node) {
    if (node && node->close) {
        node->close(node);
    }
}

/*
 * Resolve path to node
 */
struct vfs_node *vfs_resolve_path(const char *path) {
    if (!path || !vfs_root) return NULL;

    /* Handle root path */
    if (path[0] == '/' && path[1] == '\0') {
        return vfs_root;
    }

    /* Skip leading slash */
    if (path[0] == '/') {
        path++;
    }

    /* Start from root */
    struct vfs_node *current = vfs_root;

    /* Make a copy of path for tokenizing */
    char path_copy[VFS_MAX_PATH];
    strncpy(path_copy, path, VFS_MAX_PATH - 1);
    path_copy[VFS_MAX_PATH - 1] = '\0';

    /* Walk the path */
    char *saveptr = NULL;
    char *token = strtok_r(path_copy, "/", &saveptr);

    while (token && current) {
        /* Skip empty tokens */
        if (token[0] == '\0') {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        /* Handle . and .. */
        if (strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        /* Find the entry in current directory */
        current = vfs_finddir(current, token);

        token = strtok_r(NULL, "/", &saveptr);
    }

    return current;
}

/*
 * Get file size
 */
uint64_t vfs_size(struct vfs_node *node) {
    if (!node) return 0;
    return node->size;
}

/*
 * Check if node is a directory
 */
bool vfs_is_directory(struct vfs_node *node) {
    if (!node) return false;
    return (node->flags & VFS_DIRECTORY) != 0;
}

/*
 * Check if node is a file
 */
bool vfs_is_file(struct vfs_node *node) {
    if (!node) return false;
    return (node->flags & VFS_FILE) != 0;
}
