/*
 * AstraOS - FAT16 File System Implementation
 * READ-ONLY FAT16 driver
 *
 * This driver only supports READ operations.
 * No write, create, delete, or modify operations are implemented.
 */

#include "fat.h"
#include "vfs.h"
#include "../drivers/ata.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../lib/stdio.h"

/*
 * FAT16 filesystem state
 */
struct fat16_fs {
    int drive;                      /* ATA drive number */
    uint32_t partition_lba;         /* Partition start LBA */

    /* BPB info */
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint32_t total_sectors;
    uint16_t sectors_per_fat;

    /* Calculated values */
    uint32_t fat_start_lba;         /* Start of FAT */
    uint32_t root_dir_start_lba;    /* Start of root directory */
    uint32_t root_dir_sectors;      /* Sectors in root directory */
    uint32_t data_start_lba;        /* Start of data area */
    uint32_t total_clusters;        /* Total data clusters */

    /* FAT table cache */
    uint16_t *fat_table;
};

/* Global filesystem instance */
static struct fat16_fs *g_fat = NULL;

/* Static directory entry for readdir */
static struct dirent g_dirent;

/* Node cache (simple implementation) */
#define MAX_NODES 64
static struct vfs_node node_cache[MAX_NODES];
static int next_node = 0;

/*
 * Read sectors from disk
 */
static int fat_read_sectors(uint32_t lba, uint32_t count, void *buffer) {
    if (!g_fat) return -1;
    return ata_read(g_fat->drive, g_fat->partition_lba + lba, count, buffer);
}

/*
 * Get next cluster in chain
 */
static uint16_t fat_get_next_cluster(uint16_t cluster) {
    if (!g_fat || !g_fat->fat_table) return FAT16_END_MAX;
    if (cluster < 2 || cluster >= g_fat->total_clusters + 2) return FAT16_END_MAX;
    return g_fat->fat_table[cluster];
}

/*
 * Check if cluster is end of chain
 */
static bool fat_is_end_cluster(uint16_t cluster) {
    return cluster >= FAT16_END_MIN;
}

/*
 * Convert cluster number to LBA
 */
static uint32_t fat_cluster_to_lba(uint16_t cluster) {
    if (!g_fat || cluster < 2) return 0;
    return g_fat->data_start_lba + (cluster - 2) * g_fat->sectors_per_cluster;
}

/*
 * Allocate a VFS node
 */
static struct vfs_node *fat_alloc_node(void) {
    if (next_node >= MAX_NODES) {
        /* Simple wraparound - not ideal but works for demo */
        next_node = 1;  /* Keep root at 0 */
    }
    struct vfs_node *node = &node_cache[next_node++];
    memset(node, 0, sizeof(struct vfs_node));
    return node;
}

/*
 * Convert FAT 8.3 filename to normal string
 */
static void fat_name_to_string(const struct fat16_dir_entry *entry, char *out) {
    int i, j = 0;

    /* Copy name (8 chars, space padded) */
    for (i = 0; i < 8 && entry->name[i] != ' '; i++) {
        out[j++] = entry->name[i];
    }

    /* Add extension if present */
    if (entry->ext[0] != ' ') {
        out[j++] = '.';
        for (i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            out[j++] = entry->ext[i];
        }
    }

    out[j] = '\0';

    /* Convert to lowercase for convenience */
    for (i = 0; out[i]; i++) {
        if (out[i] >= 'A' && out[i] <= 'Z') {
            out[i] = out[i] - 'A' + 'a';
        }
    }
}

/*
 * Compare filename with FAT 8.3 entry (case insensitive)
 */
static bool fat_name_match(const struct fat16_dir_entry *entry, const char *name) {
    char entry_name[13];
    fat_name_to_string(entry, entry_name);

    /* Case insensitive compare */
    int i;
    for (i = 0; entry_name[i] && name[i]; i++) {
        char a = entry_name[i];
        char b = name[i];
        if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
        if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
        if (a != b) return false;
    }

    return entry_name[i] == '\0' && name[i] == '\0';
}

/* Forward declarations */
static int fat_read(struct vfs_node *node, uint64_t offset, size_t size, uint8_t *buffer);
static struct dirent *fat_readdir(struct vfs_node *node, uint32_t index);
static struct vfs_node *fat_finddir(struct vfs_node *node, const char *name);

/*
 * Create VFS node from directory entry
 */
static struct vfs_node *fat_create_node(const struct fat16_dir_entry *entry) {
    struct vfs_node *node = fat_alloc_node();
    if (!node) return NULL;

    fat_name_to_string(entry, node->name);

    if (entry->attributes & FAT_ATTR_DIRECTORY) {
        node->flags = VFS_DIRECTORY;
    } else {
        node->flags = VFS_FILE;
    }

    node->size = entry->file_size;
    node->inode = entry->cluster_low;  /* Store starting cluster as inode */
    node->impl = entry->cluster_low;

    /* Set operations */
    node->read = fat_read;
    node->readdir = fat_readdir;
    node->finddir = fat_finddir;

    return node;
}

/*
 * Read from file
 */
static int fat_read(struct vfs_node *node, uint64_t offset, size_t size, uint8_t *buffer) {
    if (!node || !buffer || !g_fat) return -1;

    /* Can't read from directory */
    if (node->flags & VFS_DIRECTORY) return -1;

    /* Check bounds */
    if (offset >= node->size) return 0;
    if (offset + size > node->size) {
        size = node->size - offset;
    }

    uint16_t cluster = (uint16_t)node->impl;
    uint32_t cluster_size = g_fat->sectors_per_cluster * g_fat->bytes_per_sector;
    uint32_t bytes_read = 0;

    /* Allocate sector buffer */
    uint8_t *sector_buf = kmalloc(cluster_size);
    if (!sector_buf) return -1;

    /* Skip to starting cluster */
    while (offset >= cluster_size && !fat_is_end_cluster(cluster)) {
        offset -= cluster_size;
        cluster = fat_get_next_cluster(cluster);
    }

    /* Read data */
    while (bytes_read < size && !fat_is_end_cluster(cluster)) {
        /* Read cluster */
        uint32_t lba = fat_cluster_to_lba(cluster);
        if (fat_read_sectors(lba, g_fat->sectors_per_cluster, sector_buf) < 0) {
            kfree(sector_buf);
            return -1;
        }

        /* Copy data from cluster */
        uint32_t cluster_offset = offset % cluster_size;
        uint32_t to_copy = cluster_size - cluster_offset;
        if (to_copy > size - bytes_read) {
            to_copy = size - bytes_read;
        }

        memcpy(buffer + bytes_read, sector_buf + cluster_offset, to_copy);
        bytes_read += to_copy;
        offset = 0;  /* After first cluster, start from beginning */

        cluster = fat_get_next_cluster(cluster);
    }

    kfree(sector_buf);
    return bytes_read;
}

/*
 * Read directory entry by index
 */
static struct dirent *fat_readdir(struct vfs_node *node, uint32_t index) {
    if (!node || !g_fat) return NULL;
    if (!(node->flags & VFS_DIRECTORY)) return NULL;

    uint8_t *sector_buf = kmalloc(g_fat->bytes_per_sector);
    if (!sector_buf) return NULL;

    uint32_t entry_count = 0;
    struct fat16_dir_entry *entry = NULL;

    /* Root directory or subdirectory? */
    if (node->impl == 0) {
        /* Root directory - fixed location */
        uint32_t root_entries = g_fat->root_entries;
        uint32_t entries_per_sector = g_fat->bytes_per_sector / sizeof(struct fat16_dir_entry);

        for (uint32_t i = 0; i < g_fat->root_dir_sectors; i++) {
            if (fat_read_sectors(g_fat->root_dir_start_lba + i, 1, sector_buf) < 0) {
                kfree(sector_buf);
                return NULL;
            }

            struct fat16_dir_entry *entries = (struct fat16_dir_entry *)sector_buf;

            for (uint32_t j = 0; j < entries_per_sector && entry_count <= index; j++) {
                entry = &entries[j];

                /* End of directory */
                if (entry->name[0] == 0x00) {
                    kfree(sector_buf);
                    return NULL;
                }

                /* Skip deleted entries */
                if ((uint8_t)entry->name[0] == 0xE5) continue;

                /* Skip volume label and LFN entries */
                if (entry->attributes & FAT_ATTR_VOLUME_ID) continue;

                if (entry_count == index) {
                    fat_name_to_string(entry, g_dirent.name);
                    g_dirent.inode = entry->cluster_low;
                    kfree(sector_buf);
                    return &g_dirent;
                }

                entry_count++;
            }
        }
    } else {
        /* Subdirectory - cluster chain */
        uint16_t cluster = (uint16_t)node->impl;
        uint32_t cluster_size = g_fat->sectors_per_cluster * g_fat->bytes_per_sector;
        uint32_t entries_per_cluster = cluster_size / sizeof(struct fat16_dir_entry);

        uint8_t *cluster_buf = kmalloc(cluster_size);
        if (!cluster_buf) {
            kfree(sector_buf);
            return NULL;
        }

        while (!fat_is_end_cluster(cluster)) {
            uint32_t lba = fat_cluster_to_lba(cluster);
            if (fat_read_sectors(lba, g_fat->sectors_per_cluster, cluster_buf) < 0) {
                kfree(cluster_buf);
                kfree(sector_buf);
                return NULL;
            }

            struct fat16_dir_entry *entries = (struct fat16_dir_entry *)cluster_buf;

            for (uint32_t j = 0; j < entries_per_cluster; j++) {
                entry = &entries[j];

                /* End of directory */
                if (entry->name[0] == 0x00) {
                    kfree(cluster_buf);
                    kfree(sector_buf);
                    return NULL;
                }

                /* Skip deleted entries */
                if ((uint8_t)entry->name[0] == 0xE5) continue;

                /* Skip volume label and LFN entries */
                if (entry->attributes & FAT_ATTR_VOLUME_ID) continue;

                if (entry_count == index) {
                    fat_name_to_string(entry, g_dirent.name);
                    g_dirent.inode = entry->cluster_low;
                    kfree(cluster_buf);
                    kfree(sector_buf);
                    return &g_dirent;
                }

                entry_count++;
            }

            cluster = fat_get_next_cluster(cluster);
        }

        kfree(cluster_buf);
    }

    kfree(sector_buf);
    return NULL;
}

/*
 * Find file/directory in a directory
 */
static struct vfs_node *fat_finddir(struct vfs_node *node, const char *name) {
    if (!node || !name || !g_fat) return NULL;
    if (!(node->flags & VFS_DIRECTORY)) return NULL;

    uint8_t *sector_buf = kmalloc(g_fat->bytes_per_sector);
    if (!sector_buf) return NULL;

    struct fat16_dir_entry *entry = NULL;

    /* Root directory or subdirectory? */
    if (node->impl == 0) {
        /* Root directory */
        uint32_t entries_per_sector = g_fat->bytes_per_sector / sizeof(struct fat16_dir_entry);

        for (uint32_t i = 0; i < g_fat->root_dir_sectors; i++) {
            if (fat_read_sectors(g_fat->root_dir_start_lba + i, 1, sector_buf) < 0) {
                kfree(sector_buf);
                return NULL;
            }

            struct fat16_dir_entry *entries = (struct fat16_dir_entry *)sector_buf;

            for (uint32_t j = 0; j < entries_per_sector; j++) {
                entry = &entries[j];

                /* End of directory */
                if (entry->name[0] == 0x00) {
                    kfree(sector_buf);
                    return NULL;
                }

                /* Skip deleted entries */
                if ((uint8_t)entry->name[0] == 0xE5) continue;

                /* Skip volume label and LFN entries */
                if (entry->attributes & FAT_ATTR_VOLUME_ID) continue;

                if (fat_name_match(entry, name)) {
                    struct vfs_node *found = fat_create_node(entry);
                    kfree(sector_buf);
                    return found;
                }
            }
        }
    } else {
        /* Subdirectory */
        uint16_t cluster = (uint16_t)node->impl;
        uint32_t cluster_size = g_fat->sectors_per_cluster * g_fat->bytes_per_sector;
        uint32_t entries_per_cluster = cluster_size / sizeof(struct fat16_dir_entry);

        uint8_t *cluster_buf = kmalloc(cluster_size);
        if (!cluster_buf) {
            kfree(sector_buf);
            return NULL;
        }

        while (!fat_is_end_cluster(cluster)) {
            uint32_t lba = fat_cluster_to_lba(cluster);
            if (fat_read_sectors(lba, g_fat->sectors_per_cluster, cluster_buf) < 0) {
                kfree(cluster_buf);
                kfree(sector_buf);
                return NULL;
            }

            struct fat16_dir_entry *entries = (struct fat16_dir_entry *)cluster_buf;

            for (uint32_t j = 0; j < entries_per_cluster; j++) {
                entry = &entries[j];

                /* End of directory */
                if (entry->name[0] == 0x00) {
                    kfree(cluster_buf);
                    kfree(sector_buf);
                    return NULL;
                }

                /* Skip deleted entries */
                if ((uint8_t)entry->name[0] == 0xE5) continue;

                /* Skip volume label and LFN entries */
                if (entry->attributes & FAT_ATTR_VOLUME_ID) continue;

                if (fat_name_match(entry, name)) {
                    struct vfs_node *found = fat_create_node(entry);
                    kfree(cluster_buf);
                    kfree(sector_buf);
                    return found;
                }
            }

            cluster = fat_get_next_cluster(cluster);
        }

        kfree(cluster_buf);
    }

    kfree(sector_buf);
    return NULL;
}

/*
 * Check if drive contains FAT16 filesystem
 */
bool fat16_detect(int drive, uint32_t partition_lba) {
    if (!ata_drive_present(drive)) return false;

    uint8_t sector[512];
    if (ata_read(drive, partition_lba, 1, sector) < 0) return false;

    struct fat16_bpb *bpb = (struct fat16_bpb *)sector;

    /* Check boot signature */
    if (sector[510] != 0x55 || sector[511] != 0xAA) return false;

    /* Check for valid BPB values */
    if (bpb->bytes_per_sector != 512) return false;
    if (bpb->num_fats == 0 || bpb->num_fats > 2) return false;
    if (bpb->root_entries == 0) return false;

    /* Check filesystem type string */
    if (memcmp(bpb->fs_type, "FAT16", 5) != 0 &&
        memcmp(bpb->fs_type, "FAT12", 5) != 0 &&
        memcmp(bpb->fs_type, "FAT     ", 8) != 0) {
        /* Not a recognized FAT type string, but might still be valid */
    }

    return true;
}

/*
 * Initialize FAT16 filesystem
 */
struct vfs_node *fat16_init(int drive, uint32_t partition_lba) {
    if (!fat16_detect(drive, partition_lba)) {
        return NULL;
    }

    /* Allocate filesystem state */
    g_fat = kmalloc(sizeof(struct fat16_fs));
    if (!g_fat) return NULL;

    memset(g_fat, 0, sizeof(struct fat16_fs));
    g_fat->drive = drive;
    g_fat->partition_lba = partition_lba;

    /* Read boot sector */
    uint8_t sector[512];
    if (ata_read(drive, partition_lba, 1, sector) < 0) {
        kfree(g_fat);
        g_fat = NULL;
        return NULL;
    }

    struct fat16_bpb *bpb = (struct fat16_bpb *)sector;

    /* Copy BPB info */
    g_fat->bytes_per_sector = bpb->bytes_per_sector;
    g_fat->sectors_per_cluster = bpb->sectors_per_cluster;
    g_fat->reserved_sectors = bpb->reserved_sectors;
    g_fat->num_fats = bpb->num_fats;
    g_fat->root_entries = bpb->root_entries;
    g_fat->sectors_per_fat = bpb->sectors_per_fat;

    if (bpb->total_sectors_16 != 0) {
        g_fat->total_sectors = bpb->total_sectors_16;
    } else {
        g_fat->total_sectors = bpb->total_sectors_32;
    }

    /* Calculate derived values */
    g_fat->fat_start_lba = g_fat->reserved_sectors;
    g_fat->root_dir_start_lba = g_fat->fat_start_lba +
                                 (g_fat->num_fats * g_fat->sectors_per_fat);
    g_fat->root_dir_sectors = (g_fat->root_entries * 32 + g_fat->bytes_per_sector - 1) /
                               g_fat->bytes_per_sector;
    g_fat->data_start_lba = g_fat->root_dir_start_lba + g_fat->root_dir_sectors;

    uint32_t data_sectors = g_fat->total_sectors - g_fat->data_start_lba;
    g_fat->total_clusters = data_sectors / g_fat->sectors_per_cluster;

    /* Load FAT table */
    uint32_t fat_bytes = g_fat->sectors_per_fat * g_fat->bytes_per_sector;
    g_fat->fat_table = kmalloc(fat_bytes);
    if (!g_fat->fat_table) {
        kfree(g_fat);
        g_fat = NULL;
        return NULL;
    }

    if (fat_read_sectors(g_fat->fat_start_lba, g_fat->sectors_per_fat,
                         g_fat->fat_table) < 0) {
        kfree(g_fat->fat_table);
        kfree(g_fat);
        g_fat = NULL;
        return NULL;
    }

    /* Create root node */
    struct vfs_node *root = &node_cache[0];
    memset(root, 0, sizeof(struct vfs_node));
    next_node = 1;

    strcpy(root->name, "/");
    root->flags = VFS_DIRECTORY;
    root->impl = 0;  /* Root directory has no cluster */
    root->read = fat_read;
    root->readdir = fat_readdir;
    root->finddir = fat_finddir;

    kprintf("FAT16: Mounted drive %d (%u clusters, %u bytes/cluster)\n",
            drive, g_fat->total_clusters,
            g_fat->sectors_per_cluster * g_fat->bytes_per_sector);

    return root;
}
