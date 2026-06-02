/*
 * AstraOS - FAT16 File System Implementation
 * FAT16 driver with read, bounded write, file creation, and file growth.
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

#define FAT16_MIN_CLUSTERS 4085
#define FAT16_MAX_CLUSTERS 65524

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
    if (count == 0) return 0;
    if (lba >= g_fat->total_sectors) return -1;
    if (count > g_fat->total_sectors - lba) return -1;
    return ata_read(g_fat->drive, g_fat->partition_lba + lba, count, buffer);
}

static bool fat_is_power_of_two(uint32_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

static uint32_t fat_bpb_total_sectors(const struct fat16_bpb *bpb) {
    if (bpb->total_sectors_16 != 0) {
        return bpb->total_sectors_16;
    }
    return bpb->total_sectors_32;
}

static bool fat_bpb_validate(const struct fat16_bpb *bpb, struct fat16_fs *fs) {
    uint32_t root_dir_bytes;
    uint32_t fat_sectors;
    uint32_t root_dir_sectors;
    uint32_t data_start_lba;
    uint32_t data_sectors;
    uint32_t total_clusters;
    uint32_t fat_entries;

    if (!bpb || !fs) return false;

    if (bpb->bytes_per_sector != 512) return false;
    if (!fat_is_power_of_two(bpb->sectors_per_cluster)) return false;
    if (bpb->sectors_per_cluster > 64) return false;
    if (bpb->reserved_sectors == 0) return false;
    if (bpb->num_fats == 0 || bpb->num_fats > 2) return false;
    if (bpb->root_entries == 0) return false;
    if (bpb->sectors_per_fat == 0) return false;
    if (bpb->total_sectors_16 != 0 && bpb->total_sectors_32 != 0) return false;

    fs->bytes_per_sector = bpb->bytes_per_sector;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->reserved_sectors = bpb->reserved_sectors;
    fs->num_fats = bpb->num_fats;
    fs->root_entries = bpb->root_entries;
    fs->sectors_per_fat = bpb->sectors_per_fat;
    fs->total_sectors = fat_bpb_total_sectors(bpb);

    if (fs->total_sectors == 0) return false;

    root_dir_bytes = (uint32_t)fs->root_entries * sizeof(struct fat16_dir_entry);
    if (root_dir_bytes == 0) return false;
    if (root_dir_bytes % fs->bytes_per_sector != 0) return false;

    fat_sectors = (uint32_t)fs->num_fats * fs->sectors_per_fat;
    if (fat_sectors / fs->num_fats != fs->sectors_per_fat) return false;
    if (fs->reserved_sectors > fs->total_sectors) return false;
    if (fat_sectors > fs->total_sectors - fs->reserved_sectors) return false;

    root_dir_sectors = root_dir_bytes / fs->bytes_per_sector;
    if (root_dir_sectors == 0) return false;

    data_start_lba = (uint32_t)fs->reserved_sectors + fat_sectors + root_dir_sectors;
    if (data_start_lba <= fs->reserved_sectors) return false;
    if (data_start_lba >= fs->total_sectors) return false;

    data_sectors = fs->total_sectors - data_start_lba;
    if (data_sectors < fs->sectors_per_cluster) return false;

    total_clusters = data_sectors / fs->sectors_per_cluster;
    if (total_clusters < FAT16_MIN_CLUSTERS) return false;
    if (total_clusters > FAT16_MAX_CLUSTERS) return false;

    fat_entries = ((uint32_t)fs->sectors_per_fat * fs->bytes_per_sector) /
                  sizeof(uint16_t);
    if (fat_entries < total_clusters + 2) return false;

    fs->fat_start_lba = fs->reserved_sectors;
    fs->root_dir_start_lba = fs->fat_start_lba + fat_sectors;
    fs->root_dir_sectors = root_dir_sectors;
    fs->data_start_lba = data_start_lba;
    fs->total_clusters = total_clusters;

    return true;
}

static bool fat_is_lfn_entry(const struct fat16_dir_entry *entry) {
    return (entry->attributes & 0x3F) == FAT_ATTR_LFN;
}

static bool fat_is_valid_data_cluster(uint16_t cluster) {
    if (!g_fat) return false;
    return cluster >= 2 && cluster < g_fat->total_clusters + 2;
}

static bool fat_should_skip_entry(const struct fat16_dir_entry *entry) {
    if ((uint8_t)entry->name[0] == 0xE5) return true;
    if (fat_is_lfn_entry(entry)) return true;
    if (entry->attributes & FAT_ATTR_VOLUME_ID) return true;
    return false;
}

/*
 * Get next cluster in chain
 */
static uint16_t fat_get_next_cluster(uint16_t cluster) {
    if (!g_fat || !g_fat->fat_table) return FAT16_END_MAX;
    if (!fat_is_valid_data_cluster(cluster)) return FAT16_END_MAX;
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
    if (!fat_is_valid_data_cluster(cluster)) return 0;
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
static int fat_write(struct vfs_node *node, uint64_t offset, size_t size, const uint8_t *buffer);
static struct dirent *fat_readdir(struct vfs_node *node, uint32_t index);
static struct vfs_node *fat_finddir(struct vfs_node *node, const char *name);
static struct vfs_node *fat_create(struct vfs_node *parent, const char *name, uint32_t uid);

static int fat_write_sectors(uint32_t lba, uint32_t count, const void *buffer) {
    if (!g_fat) return -1;
    if (count == 0) return 0;
    if (lba >= g_fat->total_sectors) return -1;
    if (count > g_fat->total_sectors - lba) return -1;
    return ata_write(g_fat->drive, g_fat->partition_lba + lba, count, buffer);
}

static bool fat_flush_table(void) {
    if (!g_fat || !g_fat->fat_table) return false;

    for (uint8_t fat = 0; fat < g_fat->num_fats; fat++) {
        uint32_t lba = g_fat->fat_start_lba + fat * g_fat->sectors_per_fat;
        if (fat_write_sectors(lba, g_fat->sectors_per_fat, g_fat->fat_table) < 0) {
            return false;
        }
    }

    return true;
}

static uint16_t fat_find_free_cluster(void) {
    if (!g_fat || !g_fat->fat_table) return FAT16_END_MAX;

    for (uint32_t cluster = 2; cluster < g_fat->total_clusters + 2; cluster++) {
        if (g_fat->fat_table[cluster] == FAT16_FREE) {
            return (uint16_t)cluster;
        }
    }

    return FAT16_END_MAX;
}

static bool fat_zero_cluster(uint16_t cluster) {
    if (!fat_is_valid_data_cluster(cluster)) return false;

    uint32_t cluster_size = g_fat->sectors_per_cluster * g_fat->bytes_per_sector;
    uint8_t *zero = kmalloc(cluster_size);
    if (!zero) return false;

    memset(zero, 0, cluster_size);
    bool ok = fat_write_sectors(fat_cluster_to_lba(cluster),
                                g_fat->sectors_per_cluster,
                                zero) >= 0;
    kfree(zero);
    return ok;
}

static bool fat_allocate_cluster(uint16_t *out_cluster) {
    uint16_t cluster = fat_find_free_cluster();
    if (!fat_is_valid_data_cluster(cluster)) return false;

    g_fat->fat_table[cluster] = FAT16_END_MAX;
    if (!fat_zero_cluster(cluster) || !fat_flush_table()) {
        g_fat->fat_table[cluster] = FAT16_FREE;
        fat_flush_table();
        return false;
    }

    *out_cluster = cluster;
    return true;
}

static bool fat_update_dir_entry(struct vfs_node *node) {
    if (!node || node->meta_lba == 0) return false;

    uint8_t sector[512];
    if (fat_read_sectors((uint32_t)node->meta_lba, 1, sector) < 0) {
        return false;
    }

    if (node->meta_offset + sizeof(struct fat16_dir_entry) > g_fat->bytes_per_sector) {
        return false;
    }

    struct fat16_dir_entry *entry =
        (struct fat16_dir_entry *)(sector + node->meta_offset);
    entry->cluster_low = (uint16_t)node->impl;
    entry->file_size = (uint32_t)node->size;

    return fat_write_sectors((uint32_t)node->meta_lba, 1, sector) >= 0;
}

static bool fat_make_83_name(const char *name, char out_name[8], char out_ext[3]) {
    if (!name || !*name) return false;

    memset(out_name, ' ', 8);
    memset(out_ext, ' ', 3);

    int ni = 0;
    int ei = 0;
    bool in_ext = false;

    for (size_t i = 0; name[i]; i++) {
        char c = name[i];
        if (c == '.') {
            if (in_ext) return false;
            in_ext = true;
            continue;
        }

        if (c <= ' ' || c == '/' || c == '\\' || c == ':' ||
            c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            return false;
        }

        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }

        if (!in_ext) {
            if (ni >= 8) return false;
            out_name[ni++] = c;
        } else {
            if (ei >= 3) return false;
            out_ext[ei++] = c;
        }
    }

    return ni > 0;
}

/*
 * Create VFS node from directory entry
 */
static struct vfs_node *fat_create_node_at(const struct fat16_dir_entry *entry,
                                           uint32_t meta_lba,
                                           uint32_t meta_offset) {
    struct vfs_node *node = fat_alloc_node();
    if (!node) return NULL;

    fat_name_to_string(entry, node->name);

    if (entry->attributes & FAT_ATTR_DIRECTORY) {
        node->flags = VFS_DIRECTORY;
        node->permissions = VFS_PERM_READ | VFS_PERM_EXEC | VFS_PERM_WRITE;
    } else {
        node->flags = VFS_FILE;
        node->permissions = VFS_PERM_READ | VFS_PERM_EXEC | VFS_PERM_WRITE;
    }

    node->size = entry->file_size;
    node->inode = entry->cluster_low;  /* Store starting cluster as inode */
    node->impl = entry->cluster_low;
    node->meta_lba = meta_lba;
    node->meta_offset = meta_offset;

    /* Set operations */
    node->read = fat_read;
    node->write = fat_write;
    node->readdir = fat_readdir;
    node->finddir = fat_finddir;
    node->create = fat_create;

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
    if (size > node->size - offset) {
        size = node->size - offset;
    }

    uint16_t cluster = (uint16_t)node->impl;
    uint32_t cluster_size = g_fat->sectors_per_cluster * g_fat->bytes_per_sector;
    uint32_t bytes_read = 0;
    uint32_t traversed = 0;

    /* Allocate sector buffer */
    uint8_t *sector_buf = kmalloc(cluster_size);
    if (!sector_buf) return -1;

    /* Skip to starting cluster */
    while (offset >= cluster_size && !fat_is_end_cluster(cluster)) {
        if (!fat_is_valid_data_cluster(cluster) || traversed >= g_fat->total_clusters) {
            kfree(sector_buf);
            return -1;
        }
        offset -= cluster_size;
        cluster = fat_get_next_cluster(cluster);
        traversed++;
    }

    /* Read data */
    while (bytes_read < size && !fat_is_end_cluster(cluster)) {
        if (!fat_is_valid_data_cluster(cluster) || traversed >= g_fat->total_clusters) {
            kfree(sector_buf);
            return -1;
        }

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
        traversed++;
    }

    kfree(sector_buf);
    return bytes_read;
}

static int fat_write(struct vfs_node *node, uint64_t offset, size_t size, const uint8_t *buffer) {
    if (!node || !buffer || !g_fat) return -1;
    if (node->flags & VFS_DIRECTORY) return -1;
    if (size == 0) return 0;
    if (offset > UINT32_MAX || size > UINT32_MAX - offset) return -1;

    if (offset > node->size) {
        uint8_t zeroes[128];
        memset(zeroes, 0, sizeof(zeroes));

        while (node->size < offset) {
            uint64_t gap = offset - node->size;
            size_t chunk = sizeof(zeroes);
            if (chunk > gap) {
                chunk = (size_t)gap;
            }
            if (fat_write(node, node->size, chunk, zeroes) != (int)chunk) {
                return -1;
            }
        }
    }

    uint32_t cluster_size = g_fat->sectors_per_cluster * g_fat->bytes_per_sector;
    uint64_t end_offset = offset + size;
    uint32_t needed_clusters = (uint32_t)((end_offset + cluster_size - 1) / cluster_size);

    if (needed_clusters > g_fat->total_clusters) return -1;

    if (needed_clusters > 0 && node->impl == 0) {
        uint16_t first;
        if (!fat_allocate_cluster(&first)) return -1;
        node->impl = first;
        node->inode = first;
    }

    uint16_t cluster = (uint16_t)node->impl;
    uint16_t last_cluster = 0;
    uint32_t current_clusters = 0;

    while (current_clusters < needed_clusters) {
        if (!fat_is_valid_data_cluster(cluster)) return -1;
        last_cluster = cluster;
        current_clusters++;

        if (current_clusters == needed_clusters) {
            break;
        }

        uint16_t next = fat_get_next_cluster(cluster);
        if (fat_is_end_cluster(next)) {
            uint16_t allocated;
            if (!fat_allocate_cluster(&allocated)) return -1;
            g_fat->fat_table[cluster] = allocated;
            g_fat->fat_table[allocated] = FAT16_END_MAX;
            if (!fat_flush_table()) return -1;
            next = allocated;
        }
        cluster = next;
    }

    (void)last_cluster;

    cluster = (uint16_t)node->impl;
    uint32_t bytes_written = 0;
    uint32_t traversed = 0;

    uint8_t *cluster_buf = kmalloc(cluster_size);
    if (!cluster_buf) return -1;

    while (offset >= cluster_size && !fat_is_end_cluster(cluster)) {
        if (!fat_is_valid_data_cluster(cluster) || traversed >= g_fat->total_clusters) {
            kfree(cluster_buf);
            return -1;
        }
        offset -= cluster_size;
        cluster = fat_get_next_cluster(cluster);
        traversed++;
    }

    while (bytes_written < size && !fat_is_end_cluster(cluster)) {
        if (!fat_is_valid_data_cluster(cluster) || traversed >= g_fat->total_clusters) {
            kfree(cluster_buf);
            return -1;
        }

        uint32_t lba = fat_cluster_to_lba(cluster);
        if (fat_read_sectors(lba, g_fat->sectors_per_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }

        uint32_t cluster_offset = offset % cluster_size;
        uint32_t to_copy = cluster_size - cluster_offset;
        if (to_copy > size - bytes_written) {
            to_copy = size - bytes_written;
        }

        memcpy(cluster_buf + cluster_offset, buffer + bytes_written, to_copy);

        if (ata_write(g_fat->drive, g_fat->partition_lba + lba,
                      g_fat->sectors_per_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }

        bytes_written += to_copy;
        offset = 0;
        cluster = fat_get_next_cluster(cluster);
        traversed++;
    }

    if (end_offset > node->size) {
        node->size = end_offset;
        if (!fat_update_dir_entry(node)) {
            kfree(cluster_buf);
            return -1;
        }
    }

    kfree(cluster_buf);
    return bytes_written;
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

                /* Skip deleted, volume label, and LFN entries */
                if (fat_should_skip_entry(entry)) continue;

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
        uint32_t traversed = 0;

        uint8_t *cluster_buf = kmalloc(cluster_size);
        if (!cluster_buf) {
            kfree(sector_buf);
            return NULL;
        }

        while (!fat_is_end_cluster(cluster)) {
            if (!fat_is_valid_data_cluster(cluster) || traversed >= g_fat->total_clusters) {
                kfree(cluster_buf);
                kfree(sector_buf);
                return NULL;
            }

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

                /* Skip deleted, volume label, and LFN entries */
                if (fat_should_skip_entry(entry)) continue;

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
            traversed++;
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

                /* Skip deleted, volume label, and LFN entries */
                if (fat_should_skip_entry(entry)) continue;

                if (fat_name_match(entry, name)) {
                    struct vfs_node *found = fat_create_node_at(
                        entry,
                        g_fat->root_dir_start_lba + i,
                        j * sizeof(struct fat16_dir_entry));
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
        uint32_t traversed = 0;

        uint8_t *cluster_buf = kmalloc(cluster_size);
        if (!cluster_buf) {
            kfree(sector_buf);
            return NULL;
        }

        while (!fat_is_end_cluster(cluster)) {
            if (!fat_is_valid_data_cluster(cluster) || traversed >= g_fat->total_clusters) {
                kfree(cluster_buf);
                kfree(sector_buf);
                return NULL;
            }

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

                /* Skip deleted, volume label, and LFN entries */
                if (fat_should_skip_entry(entry)) continue;

                if (fat_name_match(entry, name)) {
                    struct vfs_node *found = fat_create_node_at(
                        entry,
                        lba,
                        j * sizeof(struct fat16_dir_entry));
                    kfree(cluster_buf);
                    kfree(sector_buf);
                    return found;
                }
            }

            cluster = fat_get_next_cluster(cluster);
            traversed++;
        }

        kfree(cluster_buf);
    }

    kfree(sector_buf);
    return NULL;
}

static bool fat_directory_slot_is_free(const struct fat16_dir_entry *entry) {
    return entry->name[0] == 0x00 || (uint8_t)entry->name[0] == 0xE5;
}

static struct vfs_node *fat_create_in_sector(struct vfs_node *parent,
                                             const char *name,
                                             uint32_t uid,
                                             uint32_t lba,
                                             uint32_t slot) {
    (void)parent;

    uint8_t sector[512];
    if (fat_read_sectors(lba, 1, sector) < 0) {
        return NULL;
    }

    uint32_t offset = slot * sizeof(struct fat16_dir_entry);
    if (offset + sizeof(struct fat16_dir_entry) > g_fat->bytes_per_sector) {
        return NULL;
    }

    struct fat16_dir_entry *entry = (struct fat16_dir_entry *)(sector + offset);
    if (!fat_directory_slot_is_free(entry)) {
        return NULL;
    }

    char fat_name[8];
    char fat_ext[3];
    if (!fat_make_83_name(name, fat_name, fat_ext)) {
        return NULL;
    }

    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, fat_name, sizeof(entry->name));
    memcpy(entry->ext, fat_ext, sizeof(entry->ext));
    entry->attributes = FAT_ATTR_ARCHIVE;
    entry->cluster_low = 0;
    entry->file_size = 0;

    if (fat_write_sectors(lba, 1, sector) < 0) {
        return NULL;
    }

    struct vfs_node *node = fat_create_node_at(entry, lba, offset);
    if (node) {
        node->uid = uid;
    }

    return node;
}

static struct vfs_node *fat_create(struct vfs_node *parent, const char *name, uint32_t uid) {
    if (!parent || !name || !g_fat) return NULL;
    if (!(parent->flags & VFS_DIRECTORY)) return NULL;
    if (fat_finddir(parent, name)) return NULL;

    uint32_t entries_per_sector =
        g_fat->bytes_per_sector / sizeof(struct fat16_dir_entry);

    if (parent->impl == 0) {
        for (uint32_t i = 0; i < g_fat->root_dir_sectors; i++) {
            uint8_t sector[512];
            uint32_t lba = g_fat->root_dir_start_lba + i;
            if (fat_read_sectors(lba, 1, sector) < 0) {
                return NULL;
            }

            struct fat16_dir_entry *entries = (struct fat16_dir_entry *)sector;
            for (uint32_t slot = 0; slot < entries_per_sector; slot++) {
                if (fat_directory_slot_is_free(&entries[slot])) {
                    return fat_create_in_sector(parent, name, uid, lba, slot);
                }
            }
        }
        return NULL;
    }

    uint16_t cluster = (uint16_t)parent->impl;
    uint32_t traversed = 0;

    while (!fat_is_end_cluster(cluster)) {
        if (!fat_is_valid_data_cluster(cluster) || traversed >= g_fat->total_clusters) {
            return NULL;
        }

        uint32_t base_lba = fat_cluster_to_lba(cluster);
        for (uint32_t sector = 0; sector < g_fat->sectors_per_cluster; sector++) {
            uint8_t sector_buf[512];
            uint32_t lba = base_lba + sector;
            if (fat_read_sectors(lba, 1, sector_buf) < 0) {
                return NULL;
            }

            struct fat16_dir_entry *entries = (struct fat16_dir_entry *)sector_buf;
            for (uint32_t slot = 0; slot < entries_per_sector; slot++) {
                if (fat_directory_slot_is_free(&entries[slot])) {
                    return fat_create_in_sector(parent, name, uid, lba, slot);
                }
            }
        }

        cluster = fat_get_next_cluster(cluster);
        traversed++;
    }

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

    /* Check for valid BPB geometry before trusting derived values. */
    struct fat16_fs fs;
    memset(&fs, 0, sizeof(struct fat16_fs));
    if (!fat_bpb_validate(bpb, &fs)) return false;

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

    if (!fat_bpb_validate(bpb, g_fat)) {
        kfree(g_fat);
        g_fat = NULL;
        return NULL;
    }

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
    root->permissions = VFS_PERM_READ | VFS_PERM_EXEC | VFS_PERM_WRITE;
    root->impl = 0;  /* Root directory has no cluster */
    root->read = fat_read;
    root->readdir = fat_readdir;
    root->finddir = fat_finddir;
    root->create = fat_create;

    kprintf("FAT16: Mounted drive %d (%u clusters, %u bytes/cluster)\n",
            drive, g_fat->total_clusters,
            g_fat->sectors_per_cluster * g_fat->bytes_per_sector);

    return root;
}
