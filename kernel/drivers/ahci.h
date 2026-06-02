/*
 * AstraOS - AHCI PCI Probe Driver Header
 */

#ifndef _ASTRA_DRIVERS_AHCI_H
#define _ASTRA_DRIVERS_AHCI_H

#include "pci.h"

extern const struct pci_driver ahci_pci_driver;

void ahci_init(void);
int ahci_probe(const struct pci_device *dev);

#endif /* _ASTRA_DRIVERS_AHCI_H */
