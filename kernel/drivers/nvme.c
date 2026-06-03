/*
 * AstraOS - NVMe Driver
 * Minimal polling NVMe 1.x driver for one namespace per controller.
 */

#include "nvme.h"
#include "serial.h"
#include "../lib/string.h"
#include "../mm/pmm.h"

#define NVME_MAX_DRIVES        4
#define NVME_QUEUE_ENTRIES     16
#define NVME_SECTOR_SIZE       512U
#define NVME_MAX_SECTORS_IO    8U

#define PCI_COMMAND            0x04
#define PCI_COMMAND_MEMORY     (1U << 1)
#define PCI_COMMAND_BUS_MASTER (1U << 2)

#define NVME_REG_CAP           0x0000
#define NVME_REG_CC            0x0014
#define NVME_REG_CSTS          0x001C
#define NVME_REG_AQA           0x0024
#define NVME_REG_ASQ           0x0028
#define NVME_REG_ACQ           0x0030
#define NVME_REG_DBS           0x1000

#define NVME_CC_EN             (1U << 0)
#define NVME_CC_CSS_NVM        (0U << 4)
#define NVME_CC_IOSQES         (6U << 16)
#define NVME_CC_IOCQES         (4U << 20)
#define NVME_CSTS_RDY          (1U << 0)
#define NVME_CSTS_CFS          (1U << 1)

#define NVME_ADMIN_CREATE_SQ   0x01
#define NVME_ADMIN_CREATE_CQ   0x05
#define NVME_ADMIN_IDENTIFY    0x06

#define NVME_IO_WRITE          0x01
#define NVME_IO_READ           0x02

extern uint64_t hhdm_offset;

struct nvme_command {
    uint8_t opcode;
    uint8_t flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t reserved2;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
};

struct nvme_completion {
    uint32_t result;
    uint32_t reserved;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
};

struct nvme_queue {
    struct nvme_command *sq;
    struct nvme_completion *cq;
    uint64_t sq_phys;
    uint64_t cq_phys;
    uint16_t qid;
    uint16_t size;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint8_t cq_phase;
};

struct nvme_drive {
    bool present;
    volatile uint8_t *mmio;
    uint32_t doorbell_stride;
    uint32_t nsid;
    uint64_t sectors;
    struct nvme_queue adminq;
    struct nvme_queue ioq;
};

static struct nvme_drive drives[NVME_MAX_DRIVES];
static int drive_count;

static void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static uint32_t nvme_read32(struct nvme_drive *drive, uint32_t offset) {
    return *(volatile uint32_t *)(drive->mmio + offset);
}

static uint64_t nvme_read64(struct nvme_drive *drive, uint32_t offset) {
    uint64_t low = nvme_read32(drive, offset);
    uint64_t high = nvme_read32(drive, offset + 4);
    return low | (high << 32);
}

static void nvme_write32(struct nvme_drive *drive, uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(drive->mmio + offset) = value;
}

static void nvme_write64(struct nvme_drive *drive, uint32_t offset, uint64_t value) {
    nvme_write32(drive, offset, (uint32_t)value);
    nvme_write32(drive, offset + 4, (uint32_t)(value >> 32));
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

static bool nvme_alloc_queue(struct nvme_queue *queue, uint16_t qid, uint16_t size) {
    void *sq_virt = NULL;
    void *cq_virt = NULL;
    uint64_t sq_phys = alloc_page_zero(&sq_virt);
    uint64_t cq_phys = alloc_page_zero(&cq_virt);
    if (!sq_phys || !cq_phys) {
        if (sq_phys) pmm_free_page((void *)sq_phys);
        if (cq_phys) pmm_free_page((void *)cq_phys);
        return false;
    }

    queue->sq = (struct nvme_command *)sq_virt;
    queue->cq = (struct nvme_completion *)cq_virt;
    queue->sq_phys = sq_phys;
    queue->cq_phys = cq_phys;
    queue->qid = qid;
    queue->size = size;
    queue->sq_tail = 0;
    queue->cq_head = 0;
    queue->cq_phase = 1;
    return true;
}

static void nvme_ring_sq(struct nvme_drive *drive, struct nvme_queue *queue) {
    uint32_t offset = NVME_REG_DBS + (2U * queue->qid) * drive->doorbell_stride;
    nvme_write32(drive, offset, queue->sq_tail);
}

static void nvme_ring_cq(struct nvme_drive *drive, struct nvme_queue *queue) {
    uint32_t offset = NVME_REG_DBS + (2U * queue->qid + 1U) * drive->doorbell_stride;
    nvme_write32(drive, offset, queue->cq_head);
}

static int nvme_submit(struct nvme_drive *drive,
                       struct nvme_queue *queue,
                       const struct nvme_command *cmd) {
    uint16_t cid = queue->sq_tail;
    struct nvme_command *slot = &queue->sq[queue->sq_tail];
    *slot = *cmd;
    slot->cid = cid;

    queue->sq_tail++;
    if (queue->sq_tail == queue->size) {
        queue->sq_tail = 0;
    }
    nvme_ring_sq(drive, queue);

    for (uint32_t spin = 0; spin < 5000000; spin++) {
        struct nvme_completion *cqe = &queue->cq[queue->cq_head];
        uint8_t phase = (uint8_t)(cqe->status & 1);
        if (phase != queue->cq_phase) {
            continue;
        }

        uint16_t status = (uint16_t)((cqe->status >> 1) & 0x7FFF);
        queue->cq_head++;
        if (queue->cq_head == queue->size) {
            queue->cq_head = 0;
            queue->cq_phase ^= 1;
        }
        nvme_ring_cq(drive, queue);
        return status == 0 ? 0 : -1;
    }

    return -1;
}

static int nvme_admin_identify(struct nvme_drive *drive, uint32_t nsid, uint32_t cns, uint64_t data_phys) {
    struct nvme_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_IDENTIFY;
    cmd.nsid = nsid;
    cmd.prp1 = data_phys;
    cmd.cdw10 = cns;
    return nvme_submit(drive, &drive->adminq, &cmd);
}

static int nvme_admin_create_cq(struct nvme_drive *drive) {
    struct nvme_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CREATE_CQ;
    cmd.prp1 = drive->ioq.cq_phys;
    cmd.cdw10 = drive->ioq.qid | ((uint32_t)(drive->ioq.size - 1) << 16);
    cmd.cdw11 = 1;
    return nvme_submit(drive, &drive->adminq, &cmd);
}

static int nvme_admin_create_sq(struct nvme_drive *drive) {
    struct nvme_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CREATE_SQ;
    cmd.prp1 = drive->ioq.sq_phys;
    cmd.cdw10 = drive->ioq.qid | ((uint32_t)(drive->ioq.size - 1) << 16);
    cmd.cdw11 = 1 | ((uint32_t)drive->ioq.qid << 16);
    return nvme_submit(drive, &drive->adminq, &cmd);
}

static bool nvme_wait_ready(struct nvme_drive *drive, bool ready) {
    for (uint32_t i = 0; i < 5000000; i++) {
        uint32_t csts = nvme_read32(drive, NVME_REG_CSTS);
        if (csts & NVME_CSTS_CFS) {
            return false;
        }
        if (((csts & NVME_CSTS_RDY) != 0) == ready) {
            return true;
        }
    }
    return false;
}

static bool nvme_init_controller(struct nvme_drive *drive) {
    uint64_t cap = nvme_read64(drive, NVME_REG_CAP);
    uint32_t dstrd = (uint32_t)((cap >> 32) & 0x0F);
    drive->doorbell_stride = 1U << (2 + dstrd);

    nvme_write32(drive, NVME_REG_CC, nvme_read32(drive, NVME_REG_CC) & ~NVME_CC_EN);
    if (!nvme_wait_ready(drive, false)) {
        return false;
    }

    if (!nvme_alloc_queue(&drive->adminq, 0, NVME_QUEUE_ENTRIES)) {
        return false;
    }

    nvme_write32(drive, NVME_REG_AQA,
                 (NVME_QUEUE_ENTRIES - 1) | ((NVME_QUEUE_ENTRIES - 1) << 16));
    nvme_write64(drive, NVME_REG_ASQ, drive->adminq.sq_phys);
    nvme_write64(drive, NVME_REG_ACQ, drive->adminq.cq_phys);
    nvme_write32(drive, NVME_REG_CC, NVME_CC_CSS_NVM | NVME_CC_IOSQES | NVME_CC_IOCQES | NVME_CC_EN);

    return nvme_wait_ready(drive, true);
}

static bool nvme_identify_namespace(struct nvme_drive *drive) {
    void *data = NULL;
    uint64_t data_phys = alloc_page_zero(&data);
    if (!data_phys) {
        return false;
    }

    if (nvme_admin_identify(drive, 1, 0, data_phys) < 0) {
        pmm_free_page((void *)data_phys);
        return false;
    }

    uint8_t *ns = (uint8_t *)data;
    uint64_t nsze = *(uint64_t *)(ns + 0);
    uint8_t flbas = ns[26] & 0x0F;
    uint8_t lbads = ns[128 + flbas * 4 + 3];
    uint32_t block_size = 1U << lbads;
    pmm_free_page((void *)data_phys);

    if (nsze == 0 || block_size != NVME_SECTOR_SIZE) {
        return false;
    }

    drive->nsid = 1;
    drive->sectors = nsze;
    return true;
}

static bool nvme_create_io_queue(struct nvme_drive *drive) {
    if (!nvme_alloc_queue(&drive->ioq, 1, NVME_QUEUE_ENTRIES)) {
        return false;
    }
    return nvme_admin_create_cq(drive) == 0 && nvme_admin_create_sq(drive) == 0;
}

void nvme_init(void) {
    drive_count = 0;
    memset(drives, 0, sizeof(drives));
    serial_puts("NVMe: registered PCI NVM driver with polling queue I/O\n");
}

int nvme_probe(const struct pci_device *dev) {
    if (drive_count >= NVME_MAX_DRIVES) {
        return -1;
    }

    struct pci_bar bar;
    if (pci_read_bar(dev, 0, &bar) != 0 || !bar.present || bar.is_io) {
        serial_puts("NVMe: BAR0 MMIO base missing or invalid\n");
        return -1;
    }

    uint16_t command = pci_config_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER;
    pci_config_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, command);

    struct nvme_drive *drive = &drives[drive_count];
    memset(drive, 0, sizeof(*drive));
    drive->mmio = (volatile uint8_t *)phys_to_virt(bar.address);

    serial_puts("NVMe: probing controller ");
    pci_log_location(dev);
    serial_puts(" mmio=0x");
    pci_log_hex64(bar.address);
    serial_puts("\n");

    if (!nvme_init_controller(drive) ||
        !nvme_identify_namespace(drive) ||
        !nvme_create_io_queue(drive)) {
        serial_puts("NVMe: controller initialization failed or namespace is not 512-byte LBA\n");
        return -1;
    }

    drive->present = true;
    serial_puts("NVMe: usable namespace sectors=");
    pci_log_dec((uint32_t)(drive->sectors > 0xFFFFFFFFU ? 0xFFFFFFFFU : drive->sectors));
    serial_puts("\n");
    drive_count++;
    return 0;
}

int nvme_drive_count(void) {
    return drive_count;
}

bool nvme_drive_present(int index) {
    return index >= 0 && index < drive_count && drives[index].present;
}

static int nvme_rw(int index, uint64_t lba, uint32_t count, void *buffer, bool write) {
    if (count == 0) {
        return 0;
    }
    if (!nvme_drive_present(index) || !buffer) {
        return -1;
    }

    struct nvme_drive *drive = &drives[index];
    if (lba > drive->sectors || count > drive->sectors - lba) {
        return -1;
    }

    uint8_t *user = (uint8_t *)buffer;
    uint32_t done = 0;
    while (done < count) {
        uint32_t chunk = count - done;
        if (chunk > NVME_MAX_SECTORS_IO) {
            chunk = NVME_MAX_SECTORS_IO;
        }

        void *bounce = NULL;
        uint64_t bounce_phys = alloc_page_zero(&bounce);
        if (!bounce_phys) {
            return done ? (int)done : -1;
        }

        uint32_t bytes = chunk * NVME_SECTOR_SIZE;
        if (write) {
            memcpy(bounce, user + done * NVME_SECTOR_SIZE, bytes);
        }

        struct nvme_command cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.opcode = write ? NVME_IO_WRITE : NVME_IO_READ;
        cmd.nsid = drive->nsid;
        cmd.prp1 = bounce_phys;
        cmd.cdw10 = (uint32_t)(lba + done);
        cmd.cdw11 = (uint32_t)((lba + done) >> 32);
        cmd.cdw12 = chunk - 1;

        if (nvme_submit(drive, &drive->ioq, &cmd) < 0) {
            pmm_free_page((void *)bounce_phys);
            return done ? (int)done : -1;
        }

        if (!write) {
            memcpy(user + done * NVME_SECTOR_SIZE, bounce, bytes);
        }
        pmm_free_page((void *)bounce_phys);
        done += chunk;
    }

    return (int)done;
}

int nvme_read(int index, uint64_t lba, uint32_t count, void *buffer) {
    return nvme_rw(index, lba, count, buffer, false);
}

int nvme_write(int index, uint64_t lba, uint32_t count, const void *buffer) {
    return nvme_rw(index, lba, count, (void *)buffer, true);
}

const struct pci_driver nvme_pci_driver = {
    .name = "NVMe",
    .class_code = 0x01,
    .class_mask = 0xFF,
    .subclass = 0x08,
    .subclass_mask = 0xFF,
    .prog_if = 0x00,
    .prog_if_mask = 0x00,
    .probe = nvme_probe,
    .init = nvme_init,
};
