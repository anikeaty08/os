/*
 * AstraOS - Virtual File System Header
 * Abstract filesystem interface (READ-ONLY)
 */

#ifndef _ASTRA_FS_VFS_H
#define _ASTRA_FS_VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Maximum path and name lengths
 */
#define VFS_MAX_PATH    256
#define VFS_MAX_NAME    128

/*
 * File types
 */
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x03
#define VFS_BLOCKDEVICE 0x04
#define VFS_PIPE        0x05
#define VFS_SYMLINK     0x06
#define VFS_MOUNTPOINT  0x08

/*
 * Open flags (read-only filesystem, so only read flags)
 */
#define VFS_O_RDONLY    0x00

/*
 * Seek modes
 */
#define VFS_SEEK_SET    0
#define VFS_SEEK_CUR    1
#define VFS_SEEK_END    2

/*
 * Forward declarations
 */
struct vfs_node;
struct dirent;

/*
 * File operations (READ-ONLY - no write operations)
 */
typedef int (*read_fn)(struct vfs_node *node, uint64_t offset, size_t size, uint8_t *buffer);
typedef struct dirent *(*readdir_fn)(struct vfs_node *node, uint32_t index);
typedef struct vfs_node *(*finddir_fn)(struct vfs_node *node, const char *name);
typedef int (*open_fn)(struct vfs_node *node);
typedef int (*close_fn)(struct vfs_node *node);

/*
 * VFS Node (inode-like structure)
 */
struct vfs_node {
    char name[VFS_MAX_NAME];    /* File/directory name */
    uint32_t flags;             /* Node type flags */
    uint32_t permissions;       /* Permission mask (unused for now) */
    uint32_t uid;               /* Owner user ID */
    uint32_t gid;               /* Owner group ID */
    uint64_t size;              /* File size in bytes */
    uint64_t inode;             /* Inode number */
    uint64_t impl;              /* Implementation-specific data */

    /* File operations */
    read_fn read;
    readdir_fn readdir;
    finddir_fn finddir;
    open_fn open;
    close_fn close;

    /* Linked structure */
    struct vfs_node *ptr;       /* For symlinks/mountpoints */
};

/*
 * Directory entry
 */
struct dirent {
    char name[VFS_MAX_NAME];
    uint64_t inode;
};

/*
 * File descriptor
 */
struct file {
    struct vfs_node *node;
    uint64_t offset;
    uint32_t flags;
    int refcount;
};

/*
 * VFS Functions
 */

/* Initialize VFS */
void vfs_init(void);

/* Mount root filesystem */
int vfs_mount_root(struct vfs_node *root);

/* Get root node */
struct vfs_node *vfs_get_root(void);

/* Open file by path */
struct vfs_node *vfs_open(const char *path);

/* Close file */
void vfs_close(struct vfs_node *node);

/* Read from file */
int vfs_read(struct vfs_node *node, uint64_t offset, size_t size, uint8_t *buffer);

/* Read directory entry */
struct dirent *vfs_readdir(struct vfs_node *node, uint32_t index);

/* Find entry in directory */
struct vfs_node *vfs_finddir(struct vfs_node *node, const char *name);

/* Resolve path to node */
struct vfs_node *vfs_resolve_path(const char *path);

/* Get file size */
uint64_t vfs_size(struct vfs_node *node);

/* Check if node is a directory */
bool vfs_is_directory(struct vfs_node *node);

/* Check if node is a file */
bool vfs_is_file(struct vfs_node *node);

#endif /* _ASTRA_FS_VFS_H */
