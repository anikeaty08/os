/*
 * AstraOS - AHCI PCI Probe Driver Header
 */

#ifndef _ASTRA_DRIVERS_AHCI_H
#define _ASTRA_DRIVERS_AHCI_H

#include <stdbool.h>
#include <stdint.h>
#include "pci.h"

extern const struct pci_driver ahci_pci_driver;

void ahci_init(void);
int ahci_probe(const struct pci_device *dev);
int ahci_drive_count(void);
bool ahci_drive_present(int index);
int ahci_read(int index, uint64_t lba, uint32_t count, void *buffer);
int ahci_write(int index, uint64_t lba, uint32_t count, const void *buffer);

#endif /* _ASTRA_DRIVERS_AHCI_H */
