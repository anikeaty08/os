/*
 * AstraOS - ATA Disk Driver Implementation
 * PIO mode ATA driver (READ-ONLY)
 *
 * This driver only supports READ operations.
 * Write functions are intentionally not implemented to prevent
 * accidental data corruption.
 */

#include "ata.h"
#include "../arch/x86_64/io.h"
#include "../lib/string.h"
#include "../lib/stdio.h"

/*
 * Drive information storage
 */
static struct ata_drive drives[4];

/*
 * Wait for drive to be ready (not busy)
 */
static bool ata_wait_ready(uint16_t base_port) {
    int timeout = 100000;

    while (timeout--) {
        uint8_t status = inb(base_port + 7);

        if (status & ATA_STATUS_ERR) {
            return false;
        }

        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_RDY)) {
            return true;
        }
    }

    return false;
}

/*
 * Wait for data request ready
 */
static bool ata_wait_drq(uint16_t base_port) {
    int timeout = 100000;

    while (timeout--) {
        uint8_t status = inb(base_port + 7);

        if (status & ATA_STATUS_ERR) {
            return false;
        }

        if (status & ATA_STATUS_DRQ) {
            return true;
        }
    }

    return false;
}

/*
 * Software reset
 */
static void ata_soft_reset(uint16_t control_port) {
    outb(control_port, 0x04);  /* Set SRST bit */
    io_wait();
    io_wait();
    io_wait();
    io_wait();
    outb(control_port, 0x00);  /* Clear SRST bit */
    io_wait();
}

/*
 * Select drive
 */
static void ata_select_drive(uint16_t base_port, bool slave) {
    outb(base_port + 6, slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER);
    /* Wait 400ns for drive select to take effect */
    for (int i = 0; i < 4; i++) {
        inb(base_port + 7);
    }
}

/*
 * Identify drive
 */
static bool ata_identify(int drive_num) {
    struct ata_drive *drive = &drives[drive_num];

    uint16_t base_port = (drive_num < 2) ? ATA_PRIMARY_DATA : ATA_SECONDARY_DATA;
    uint16_t control_port = (drive_num < 2) ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL;
    bool is_slave = (drive_num % 2) == 1;

    drive->base_port = base_port;
    drive->control_port = control_port;
    drive->is_master = !is_slave;
    drive->present = false;

    /* Select drive */
    ata_select_drive(base_port, is_slave);

    /* Check if drive exists */
    outb(base_port + 2, 0);  /* Sector count = 0 */
    outb(base_port + 3, 0);  /* LBA lo = 0 */
    outb(base_port + 4, 0);  /* LBA mid = 0 */
    outb(base_port + 5, 0);  /* LBA hi = 0 */

    /* Send IDENTIFY command */
    outb(base_port + 7, ATA_CMD_IDENTIFY);

    /* Check response */
    uint8_t status = inb(base_port + 7);
    if (status == 0) {
        /* No drive */
        return false;
    }

    /* Wait for BSY to clear */
    int timeout = 100000;
    while (timeout--) {
        status = inb(base_port + 7);

        if (status & ATA_STATUS_ERR) {
            /* Not ATA drive (might be ATAPI) */
            return false;
        }

        if (!(status & ATA_STATUS_BSY)) {
            break;
        }
    }

    if (timeout <= 0) {
        return false;
    }

    /* Check LBA mid and LBA hi to differentiate ATA from ATAPI */
    uint8_t lba_mid = inb(base_port + 4);
    uint8_t lba_hi = inb(base_port + 5);

    if (lba_mid != 0 || lba_hi != 0) {
        /* Not ATA */
        return false;
    }

    /* Wait for DRQ or ERR */
    if (!ata_wait_drq(base_port)) {
        return false;
    }

    /* Read identification data */
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(base_port);
    }

    /* Extract information */
    drive->present = true;

    /* Total sectors (LBA28) */
    drive->sectors = identify_data[60] | ((uint32_t)identify_data[61] << 16);

    /* Check for LBA48 support */
    if (identify_data[83] & (1 << 10)) {
        /* LBA48 supported - use 48-bit sector count */
        drive->sectors = identify_data[100] |
                        ((uint64_t)identify_data[101] << 16) |
                        ((uint64_t)identify_data[102] << 32) |
                        ((uint64_t)identify_data[103] << 48);
    }

    /* Model string (swap bytes) */
    for (int i = 0; i < 20; i++) {
        drive->model[i * 2] = identify_data[27 + i] >> 8;
        drive->model[i * 2 + 1] = identify_data[27 + i] & 0xFF;
    }
    drive->model[40] = '\0';

    /* Trim trailing spaces */
    for (int i = 39; i >= 0 && drive->model[i] == ' '; i--) {
        drive->model[i] = '\0';
    }

    /* Serial number (swap bytes) */
    for (int i = 0; i < 10; i++) {
        drive->serial[i * 2] = identify_data[10 + i] >> 8;
        drive->serial[i * 2 + 1] = identify_data[10 + i] & 0xFF;
    }
    drive->serial[20] = '\0';

    /* Trim trailing spaces */
    for (int i = 19; i >= 0 && drive->serial[i] == ' '; i--) {
        drive->serial[i] = '\0';
    }

    return true;
}

/*
 * Initialize ATA subsystem
 */
void ata_init(void) {
    /* Clear drive info */
    memset(drives, 0, sizeof(drives));

    /* Reset both buses */
    ata_soft_reset(ATA_PRIMARY_CONTROL);
    ata_soft_reset(ATA_SECONDARY_CONTROL);

    /* Identify all drives */
    for (int i = 0; i < 4; i++) {
        if (ata_identify(i)) {
            kprintf("ATA: Drive %d: %s (%llu MB)\n",
                    i, drives[i].model,
                    (drives[i].sectors * ATA_SECTOR_SIZE) / (1024 * 1024));
        }
    }
}

/*
 * Check if drive is present
 */
bool ata_drive_present(int drive) {
    if (drive < 0 || drive > 3) return false;
    return drives[drive].present;
}

/*
 * Read sectors using LBA28
 */
static int ata_read_lba28(struct ata_drive *drive, uint32_t lba, uint32_t count, void *buffer) {
    uint16_t base_port = drive->base_port;
    uint8_t *buf = (uint8_t *)buffer;

    for (uint32_t i = 0; i < count; i++) {
        /* Select drive and set LBA mode + upper 4 bits of LBA */
        uint8_t drive_sel = drive->is_master ? 0xE0 : 0xF0;
        drive_sel |= ((lba + i) >> 24) & 0x0F;
        outb(base_port + 6, drive_sel);

        /* Wait for drive ready */
        if (!ata_wait_ready(base_port)) {
            return -1;
        }

        /* Set sector count */
        outb(base_port + 2, 1);

        /* Set LBA */
        outb(base_port + 3, (lba + i) & 0xFF);
        outb(base_port + 4, ((lba + i) >> 8) & 0xFF);
        outb(base_port + 5, ((lba + i) >> 16) & 0xFF);

        /* Send READ command */
        outb(base_port + 7, ATA_CMD_READ_PIO);

        /* Wait for data */
        if (!ata_wait_drq(base_port)) {
            return -1;
        }

        /* Read sector data */
        uint16_t *wbuf = (uint16_t *)(buf + i * ATA_SECTOR_SIZE);
        for (int j = 0; j < 256; j++) {
            wbuf[j] = inw(base_port);
        }
    }

    return count;
}

/*
 * Read sectors from disk
 */
int ata_read(int drive_num, uint64_t lba, uint32_t count, void *buffer) {
    if (drive_num < 0 || drive_num > 3) {
        return -1;
    }

    struct ata_drive *drive = &drives[drive_num];
    if (!drive->present) {
        return -1;
    }

    if (lba + count > drive->sectors) {
        return -1;
    }

    if (!buffer || count == 0) {
        return 0;
    }

    /* Use LBA28 for now (supports up to 128GB) */
    if (lba + count < 0x10000000) {
        return ata_read_lba28(drive, (uint32_t)lba, count, buffer);
    }

    /* LBA48 would be needed for larger drives - not implemented */
    return -1;
}

/*
 * Get drive information
 */
struct ata_drive *ata_get_drive(int drive) {
    if (drive < 0 || drive > 3) return NULL;
    return &drives[drive];
}
