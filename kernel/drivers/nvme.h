/*
 * AstraOS - NVMe PCI Probe Driver Header
 */

#ifndef _ASTRA_DRIVERS_NVME_H
#define _ASTRA_DRIVERS_NVME_H

#include "pci.h"

extern const struct pci_driver nvme_pci_driver;

void nvme_init(void);
int nvme_probe(const struct pci_device *dev);

#endif /* _ASTRA_DRIVERS_NVME_H */
