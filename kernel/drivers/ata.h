/*
 * AstraOS - ATA Disk Driver Header
 * PIO mode ATA driver (READ-ONLY)
 */

#ifndef _ASTRA_DRIVERS_ATA_H
#define _ASTRA_DRIVERS_ATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * ATA I/O Ports (Primary Bus)
 */
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECCOUNT    0x1F2
#define ATA_PRIMARY_LBA_LO      0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HI      0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_COMMAND     0x1F7

/*
 * ATA I/O Ports (Secondary Bus)
 */
#define ATA_SECONDARY_DATA      0x170
#define ATA_SECONDARY_ERROR     0x171
#define ATA_SECONDARY_SECCOUNT  0x172
#define ATA_SECONDARY_LBA_LO    0x173
#define ATA_SECONDARY_LBA_MID   0x174
#define ATA_SECONDARY_LBA_HI    0x175
#define ATA_SECONDARY_DRIVE     0x176
#define ATA_SECONDARY_STATUS    0x177
#define ATA_SECONDARY_COMMAND   0x177

/*
 * ATA Control Ports
 */
#define ATA_PRIMARY_CONTROL     0x3F6
#define ATA_SECONDARY_CONTROL   0x376

/*
 * ATA Status Register Bits
 */
#define ATA_STATUS_ERR          (1 << 0)    /* Error */
#define ATA_STATUS_IDX          (1 << 1)    /* Index */
#define ATA_STATUS_CORR         (1 << 2)    /* Corrected data */
#define ATA_STATUS_DRQ          (1 << 3)    /* Data request ready */
#define ATA_STATUS_SRV          (1 << 4)    /* Overlapped mode service request */
#define ATA_STATUS_DF           (1 << 5)    /* Drive fault */
#define ATA_STATUS_RDY          (1 << 6)    /* Ready */
#define ATA_STATUS_BSY          (1 << 7)    /* Busy */

/*
 * ATA Commands
 */
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_WRITE_PIO       0x30        /* NOT USED - read only */
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_CACHE_FLUSH     0xE7

/*
 * Drive selection
 */
#define ATA_DRIVE_MASTER        0xA0
#define ATA_DRIVE_SLAVE         0xB0

/*
 * Sector size
 */
#define ATA_SECTOR_SIZE         512

/*
 * ATA drive information
 */
struct ata_drive {
    bool present;
    bool is_master;
    uint16_t base_port;
    uint16_t control_port;
    uint64_t sectors;
    char model[41];
    char serial[21];
};

/*
 * Initialize ATA subsystem
 */
void ata_init(void);

/*
 * Check if drive is present
 */
bool ata_drive_present(int drive);

/*
 * Read sectors from disk (READ-ONLY operation)
 *
 * drive: 0 = primary master, 1 = primary slave,
 *        2 = secondary master, 3 = secondary slave
 * lba: Logical Block Address (sector number)
 * count: Number of sectors to read
 * buffer: Destination buffer (must be count * 512 bytes)
 *
 * Returns: Number of sectors read, or -1 on error
 */
int ata_read(int drive, uint64_t lba, uint32_t count, void *buffer);

/*
 * Get drive information
 */
struct ata_drive *ata_get_drive(int drive);

#endif /* _ASTRA_DRIVERS_ATA_H */
