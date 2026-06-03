/*
 * AstraOS - NVMe PCI Probe Driver Header
 */

#ifndef _ASTRA_DRIVERS_NVME_H
#define _ASTRA_DRIVERS_NVME_H

#include <stdbool.h>
#include <stdint.h>
#include "pci.h"

extern const struct pci_driver nvme_pci_driver;

void nvme_init(void);
int nvme_probe(const struct pci_device *dev);
int nvme_drive_count(void);
bool nvme_drive_present(int index);
int nvme_read(int index, uint64_t lba, uint32_t count, void *buffer);
int nvme_write(int index, uint64_t lba, uint32_t count, const void *buffer);

#endif /* _ASTRA_DRIVERS_NVME_H */
