/*
 * AstraOS - FAT16 File System Header
 * READ-ONLY FAT16 implementation
 */

#ifndef _ASTRA_FS_FAT_H
#define _ASTRA_FS_FAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vfs.h"

/*
 * FAT16 BIOS Parameter Block (BPB)
 * Located at offset 0 of the boot sector
 */
struct fat16_bpb {
    uint8_t  jmp[3];            /* Jump instruction */
    char     oem_name[8];       /* OEM identifier */
    uint16_t bytes_per_sector;  /* Usually 512 */
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;  /* Number of reserved sectors */
    uint8_t  num_fats;          /* Number of FATs (usually 2) */
    uint16_t root_entries;      /* Max root directory entries */
    uint16_t total_sectors_16;  /* Total sectors (if < 65536) */
    uint8_t  media_type;        /* Media descriptor */
    uint16_t sectors_per_fat;   /* Sectors per FAT */
    uint16_t sectors_per_track; /* Sectors per track */
    uint16_t num_heads;         /* Number of heads */
    uint32_t hidden_sectors;    /* Hidden sectors */
    uint32_t total_sectors_32;  /* Total sectors (if >= 65536) */

    /* Extended BPB */
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;    /* 0x29 if extended BPB is valid */
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];        /* "FAT16   " */
} __attribute__((packed));

/*
 * FAT16 Directory Entry
 */
struct fat16_dir_entry {
    char     name[8];           /* Filename (space padded) */
    char     ext[3];            /* Extension (space padded) */
    uint8_t  attributes;        /* File attributes */
    uint8_t  reserved;
    uint8_t  create_time_ms;    /* Creation time (10ms units) */
    uint16_t create_time;       /* Creation time */
    uint16_t create_date;       /* Creation date */
    uint16_t access_date;       /* Last access date */
    uint16_t cluster_high;      /* High 16 bits of cluster (FAT32) */
    uint16_t modify_time;       /* Last modification time */
    uint16_t modify_date;       /* Last modification date */
    uint16_t cluster_low;       /* Starting cluster */
    uint32_t file_size;         /* File size in bytes */
} __attribute__((packed));

/*
 * File attributes
 */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F    /* Long filename entry */

/*
 * Special cluster values
 */
#define FAT16_FREE          0x0000
#define FAT16_RESERVED      0x0001
#define FAT16_BAD           0xFFF7
#define FAT16_END_MIN       0xFFF8
#define FAT16_END_MAX       0xFFFF

/*
 * Initialize FAT16 filesystem
 * drive: ATA drive number (0-3)
 * partition_lba: Starting LBA of partition (0 for no partition table)
 *
 * Returns: VFS root node for this filesystem, or NULL on error
 */
struct vfs_node *fat16_init(int drive, uint32_t partition_lba);

/*
 * Check if a drive contains FAT16 filesystem
 */
bool fat16_detect(int drive, uint32_t partition_lba);

#endif /* _ASTRA_FS_FAT_H */
