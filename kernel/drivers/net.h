/*
 * AstraOS - Network PCI Probe Driver Header
 */

#ifndef _ASTRA_DRIVERS_NET_H
#define _ASTRA_DRIVERS_NET_H

#include "pci.h"

extern const struct pci_driver net_pci_driver;

void net_init(void);
int net_probe(const struct pci_device *dev);

#endif /* _ASTRA_DRIVERS_NET_H */
