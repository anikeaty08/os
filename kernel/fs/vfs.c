/*
 * AstraOS - Virtual File System Implementation
 * Abstract filesystem interface
 *
 * This VFS supports read, write, create, delete, rename, truncate, and fsck
 * operations when the mounted filesystem exposes them.
 */

#include "vfs.h"
#include "../lib/string.h"
#include "../mm/heap.h"

/*
 * Root filesystem node
 */
static struct vfs_node *vfs_root = NULL;

static bool vfs_split_path(const char *path,
                           char parent_path[VFS_MAX_PATH],
                           char name[VFS_MAX_NAME]) {
    if (!path || !parent_path || !name || path[0] == '\0') return false;

    char path_copy[VFS_MAX_PATH];
    strncpy(path_copy, path, VFS_MAX_PATH - 1);
    path_copy[VFS_MAX_PATH - 1] = '\0';

    size_t len = strlen(path_copy);
    while (len > 1 && path_copy[len - 1] == '/') {
        path_copy[--len] = '\0';
    }

    if (strcmp(path_copy, "/") == 0) return false;

    char *last_slash = strrchr(path_copy, '/');
    char *base = path_copy;

    if (last_slash) {
        base = last_slash + 1;
        if (last_slash == path_copy) {
            strcpy(parent_path, "/");
        } else {
            *last_slash = '\0';
            strncpy(parent_path, path_copy, VFS_MAX_PATH - 1);
            parent_path[VFS_MAX_PATH - 1] = '\0';
        }
    } else {
        strcpy(parent_path, "/");
    }

    if (base[0] == '\0') return false;

    strncpy(name, base, VFS_MAX_NAME - 1);
    name[VFS_MAX_NAME - 1] = '\0';
    return true;
}

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
 * Create a regular file
 */
struct vfs_node *vfs_create(const char *path, uint32_t uid) {
    if (!path || !vfs_root) return NULL;

    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];
    if (!vfs_split_path(path, parent_path, name)) return NULL;

    struct vfs_node *parent = vfs_resolve_path(parent_path);
    if (!parent || name[0] == '\0') return NULL;
    if (!vfs_is_directory(parent) || !vfs_can_write(parent)) return NULL;
    if (vfs_finddir(parent, name)) return NULL;
    if (!parent->create) return NULL;

    return parent->create(parent, name, uid);
}

int vfs_unlink(const char *path) {
    if (!path || !vfs_root) return -1;

    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];
    if (!vfs_split_path(path, parent_path, name)) return -1;

    struct vfs_node *parent = vfs_resolve_path(parent_path);
    if (!parent || !vfs_is_directory(parent) || !vfs_can_write(parent)) return -1;
    if (!parent->unlink) return -1;

    return parent->unlink(parent, name);
}

int vfs_rename(const char *old_path, const char *new_path) {
    if (!old_path || !new_path || !vfs_root) return -1;

    char old_parent_path[VFS_MAX_PATH];
    char old_name[VFS_MAX_NAME];
    if (!vfs_split_path(old_path, old_parent_path, old_name)) return -1;

    struct vfs_node *old_parent = vfs_resolve_path(old_parent_path);
    if (!old_parent || !vfs_is_directory(old_parent) ||
        !vfs_can_write(old_parent) || !old_parent->rename) {
        return -1;
    }

    char new_parent_path[VFS_MAX_PATH];
    char new_name[VFS_MAX_NAME];
    struct vfs_node *existing = vfs_resolve_path(new_path);
    if (existing && vfs_is_directory(existing)) {
        strncpy(new_parent_path, new_path, VFS_MAX_PATH - 1);
        new_parent_path[VFS_MAX_PATH - 1] = '\0';
        strncpy(new_name, old_name, VFS_MAX_NAME - 1);
        new_name[VFS_MAX_NAME - 1] = '\0';
    } else {
        if (existing) return -1;
        if (!vfs_split_path(new_path, new_parent_path, new_name)) return -1;
    }

    struct vfs_node *new_parent = vfs_resolve_path(new_parent_path);
    if (!new_parent || !vfs_is_directory(new_parent) ||
        !vfs_can_write(new_parent)) {
        return -1;
    }

    if (vfs_finddir(new_parent, new_name)) return -1;

    return old_parent->rename(old_parent, old_name, new_parent, new_name);
}

int vfs_truncate(struct vfs_node *node, uint64_t size) {
    if (!node || !vfs_is_file(node) || !vfs_can_write(node)) return -1;
    if (!node->truncate) return -1;
    return node->truncate(node, size);
}

int vfs_fsck(uint32_t flags) {
    if (!vfs_root || !vfs_root->fsck) return -1;
    return vfs_root->fsck(vfs_root, flags);
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
