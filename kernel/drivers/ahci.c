/*
 * AstraOS - AHCI SATA Driver
 * Polling AHCI disk I/O for boot-time and filesystem sector access.
 */

#include "ahci.h"
#include "serial.h"
#include "../lib/string.h"
#include "../mm/pmm.h"

#define AHCI_MAX_DRIVES          8
#define AHCI_MAX_SECTORS_IO      8U
#define AHCI_SECTOR_SIZE         512U

#define PCI_COMMAND              0x04
#define PCI_COMMAND_IO           (1U << 0)
#define PCI_COMMAND_MEMORY       (1U << 1)
#define PCI_COMMAND_BUS_MASTER   (1U << 2)

#define HBA_PORT_DET_PRESENT     3
#define HBA_PORT_IPM_ACTIVE      1
#define HBA_PORT_SIG_ATA         0x00000101U

#define HBA_PxCMD_ST             (1U << 0)
#define HBA_PxCMD_FRE            (1U << 4)
#define HBA_PxCMD_FR             (1U << 14)
#define HBA_PxCMD_CR             (1U << 15)
#define HBA_PxIS_TFES            (1U << 30)

#define ATA_CMD_IDENTIFY         0xEC
#define ATA_CMD_READ_DMA_EXT     0x25
#define ATA_CMD_WRITE_DMA_EXT    0x35
#define ATA_DEV_BUSY             0x80
#define ATA_DEV_DRQ              0x08

#define FIS_TYPE_REG_H2D         0x27

extern uint64_t hhdm_offset;

struct hba_port {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
};

struct hba_mem {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
    struct hba_port ports[32];
} __attribute__((packed));

struct hba_cmd_header {
    uint8_t cfl;
    uint8_t flags;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed));

struct hba_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc_i;
} __attribute__((packed));

struct hba_cmd_table {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    struct hba_prdt_entry prdt[1];
} __attribute__((packed));

struct ahci_drive {
    bool present;
    struct hba_port *port;
    uint8_t port_index;
    uint64_t sectors;
    char model[41];
};

static struct ahci_drive drives[AHCI_MAX_DRIVES];
static int drive_count;

static void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static uint64_t alloc_page_zero(void **virt_out) {
    void *phys = pmm_alloc_page();
    if (!phys) {
        return 0;
    }
    void *virt = phys_to_virt((uint64_t)phys);
    memset(virt, 0, PAGE_SIZE);
    *virt_out = virt;
    return (uint64_t)phys;
}

static void ahci_stop_port(struct hba_port *port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;

    for (uint32_t i = 0; i < 100000; i++) {
        if ((port->cmd & (HBA_PxCMD_FR | HBA_PxCMD_CR)) == 0) {
            return;
        }
    }
}

static void ahci_start_port(struct hba_port *port) {
    for (uint32_t i = 0; i < 100000 && (port->cmd & HBA_PxCMD_CR); i++) {
    }
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

static int ahci_find_slot(struct hba_port *port) {
    uint32_t busy = port->sact | port->ci;
    for (int i = 0; i < 32; i++) {
        if ((busy & (1U << i)) == 0) {
            return i;
        }
    }
    return -1;
}

static int ahci_wait_ready(struct hba_port *port) {
    for (uint32_t i = 0; i < 1000000; i++) {
        if ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) == 0) {
            return 0;
        }
    }
    return -1;
}

static int ahci_issue(struct ahci_drive *drive,
                      uint8_t command,
                      uint64_t lba,
                      uint16_t count,
                      uint64_t buffer_phys,
                      uint32_t bytes,
                      bool write) {
    struct hba_port *port = drive->port;
    if (ahci_wait_ready(port) < 0) {
        return -1;
    }

    int slot = ahci_find_slot(port);
    if (slot < 0) {
        return -1;
    }

    uint64_t header_phys = ((uint64_t)port->clbu << 32) | port->clb;
    struct hba_cmd_header *headers = (struct hba_cmd_header *)phys_to_virt(header_phys);
    struct hba_cmd_header *header = &headers[slot];
    memset(header, 0, sizeof(*header));
    header->cfl = 5;
    header->flags = write ? (1U << 6) : 0;
    header->prdtl = 1;

    void *table_virt = NULL;
    uint64_t table_phys = alloc_page_zero(&table_virt);
    if (!table_phys) {
        return -1;
    }

    header->ctba = (uint32_t)table_phys;
    header->ctbau = (uint32_t)(table_phys >> 32);

    struct hba_cmd_table *table = (struct hba_cmd_table *)table_virt;
    table->prdt[0].dba = (uint32_t)buffer_phys;
    table->prdt[0].dbau = (uint32_t)(buffer_phys >> 32);
    table->prdt[0].dbc_i = (bytes - 1) | (1U << 31);

    uint8_t *fis = table->cfis;
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = 1U << 7;
    fis[2] = command;
    fis[4] = (uint8_t)lba;
    fis[5] = (uint8_t)(lba >> 8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 1U << 6;
    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);
    fis[12] = (uint8_t)count;
    fis[13] = (uint8_t)(count >> 8);

    port->is = 0xFFFFFFFFU;
    port->ci = 1U << slot;

    for (uint32_t spin = 0; spin < 5000000; spin++) {
        if ((port->ci & (1U << slot)) == 0) {
            pmm_free_page((void *)table_phys);
            return (port->is & HBA_PxIS_TFES) ? -1 : 0;
        }
        if (port->is & HBA_PxIS_TFES) {
            pmm_free_page((void *)table_phys);
            return -1;
        }
    }

    pmm_free_page((void *)table_phys);
    return -1;
}

static void copy_ident_string(char *dst, const uint16_t *id, uint32_t start, uint32_t words) {
    for (uint32_t i = 0; i < words; i++) {
        dst[i * 2] = (char)(id[start + i] >> 8);
        dst[i * 2 + 1] = (char)(id[start + i] & 0xFF);
    }
    dst[words * 2] = '\0';
}

static bool ahci_identify(struct ahci_drive *drive) {
    void *buf_virt = NULL;
    uint64_t buf_phys = alloc_page_zero(&buf_virt);
    if (!buf_phys) {
        return false;
    }

    if (ahci_issue(drive, ATA_CMD_IDENTIFY, 0, 1, buf_phys, AHCI_SECTOR_SIZE, false) < 0) {
        pmm_free_page((void *)buf_phys);
        return false;
    }

    uint16_t *id = (uint16_t *)buf_virt;
    drive->sectors = ((uint64_t)id[103] << 48) |
                     ((uint64_t)id[102] << 32) |
                     ((uint64_t)id[101] << 16) |
                     id[100];
    if (drive->sectors == 0) {
        drive->sectors = ((uint32_t)id[61] << 16) | id[60];
    }
    copy_ident_string(drive->model, id, 27, 20);
    pmm_free_page((void *)buf_phys);
    return drive->sectors != 0;
}

static bool configure_port(struct hba_mem *abar, uint8_t port_index) {
    if (drive_count >= AHCI_MAX_DRIVES) {
        return false;
    }

    struct hba_port *port = &abar->ports[port_index];
    uint32_t ssts = port->ssts;
    uint8_t det = (uint8_t)(ssts & 0x0F);
    uint8_t ipm = (uint8_t)((ssts >> 8) & 0x0F);
    if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE || port->sig != HBA_PORT_SIG_ATA) {
        return false;
    }

    void *cmd_virt = NULL;
    void *fis_virt = NULL;
    uint64_t cmd_phys = alloc_page_zero(&cmd_virt);
    uint64_t fis_phys = alloc_page_zero(&fis_virt);
    if (!cmd_phys || !fis_phys) {
        if (cmd_phys) pmm_free_page((void *)cmd_phys);
        if (fis_phys) pmm_free_page((void *)fis_phys);
        return false;
    }
    (void)cmd_virt;
    (void)fis_virt;

    ahci_stop_port(port);
    port->clb = (uint32_t)cmd_phys;
    port->clbu = (uint32_t)(cmd_phys >> 32);
    port->fb = (uint32_t)fis_phys;
    port->fbu = (uint32_t)(fis_phys >> 32);
    port->serr = 0xFFFFFFFFU;
    port->is = 0xFFFFFFFFU;
    ahci_start_port(port);

    struct ahci_drive *drive = &drives[drive_count];
    memset(drive, 0, sizeof(*drive));
    drive->present = true;
    drive->port = port;
    drive->port_index = port_index;

    if (!ahci_identify(drive)) {
        drive->present = false;
        return false;
    }

    serial_puts("AHCI: disk ");
    pci_log_dec((uint32_t)drive_count);
    serial_puts(" port=");
    pci_log_dec(port_index);
    serial_puts(" sectors=");
    pci_log_dec((uint32_t)(drive->sectors > 0xFFFFFFFFU ? 0xFFFFFFFFU : drive->sectors));
    serial_puts(" model=\"");
    serial_puts(drive->model);
    serial_puts("\"\n");
    drive_count++;
    return true;
}

void ahci_init(void) {
    drive_count = 0;
    memset(drives, 0, sizeof(drives));
    serial_puts("AHCI: registered PCI SATA driver with polling DMA sector I/O\n");
}

int ahci_probe(const struct pci_device *dev) {
    struct pci_bar bar;
    if (pci_read_bar(dev, 5, &bar) != 0 || !bar.present || bar.is_io) {
        serial_puts("AHCI: BAR5 ABAR missing or invalid\n");
        return -1;
    }

    uint16_t command = pci_config_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER | PCI_COMMAND_IO;
    pci_config_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, command);

    struct hba_mem *abar = (struct hba_mem *)phys_to_virt(bar.address);
    abar->ghc |= (1U << 31);

    uint32_t pi = abar->pi;
    uint32_t found = 0;
    serial_puts("AHCI: probing controller ");
    pci_log_location(dev);
    serial_puts(" ports=0x");
    pci_log_hex32(pi);
    serial_puts("\n");

    for (uint8_t i = 0; i < 32; i++) {
        if ((pi & (1U << i)) && configure_port(abar, i)) {
            found++;
        }
    }

    serial_puts("AHCI: usable SATA disks=");
    pci_log_dec(found);
    serial_puts("\n");
    return 0;
}

int ahci_drive_count(void) {
    return drive_count;
}

bool ahci_drive_present(int index) {
    return index >= 0 && index < drive_count && drives[index].present;
}

static int ahci_rw(int index, uint64_t lba, uint32_t count, void *buffer, bool write) {
    if (count == 0) {
        return 0;
    }
    if (!ahci_drive_present(index) || !buffer) {
        return -1;
    }

    struct ahci_drive *drive = &drives[index];
    if (lba > drive->sectors || count > drive->sectors - lba) {
        return -1;
    }

    uint8_t *user = (uint8_t *)buffer;
    uint32_t done = 0;
    while (done < count) {
        uint32_t chunk = count - done;
        if (chunk > AHCI_MAX_SECTORS_IO) {
            chunk = AHCI_MAX_SECTORS_IO;
        }

        void *bounce_virt = NULL;
        uint32_t bytes = chunk * AHCI_SECTOR_SIZE;
        uint64_t bounce_phys = alloc_page_zero(&bounce_virt);
        if (!bounce_phys) {
            return done ? (int)done : -1;
        }

        if (write) {
            memcpy(bounce_virt, user + done * AHCI_SECTOR_SIZE, bytes);
        }

        int rc = ahci_issue(drive,
                            write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT,
                            lba + done,
                            (uint16_t)chunk,
                            bounce_phys,
                            bytes,
                            write);
        if (rc < 0) {
            pmm_free_page((void *)bounce_phys);
            return done ? (int)done : -1;
        }

        if (!write) {
            memcpy(user + done * AHCI_SECTOR_SIZE, bounce_virt, bytes);
        }
        pmm_free_page((void *)bounce_phys);
        done += chunk;
    }

    return (int)done;
}

int ahci_read(int index, uint64_t lba, uint32_t count, void *buffer) {
    return ahci_rw(index, lba, count, buffer, false);
}

int ahci_write(int index, uint64_t lba, uint32_t count, const void *buffer) {
    return ahci_rw(index, lba, count, (void *)buffer, true);
}
