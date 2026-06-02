/*
 * AstraOS - USB PCI Probe Driver Header
 */

#ifndef _ASTRA_DRIVERS_USB_H
#define _ASTRA_DRIVERS_USB_H

#include "pci.h"

extern const struct pci_driver uhci_pci_driver;
extern const struct pci_driver ohci_pci_driver;
extern const struct pci_driver ehci_pci_driver;
extern const struct pci_driver xhci_pci_driver;

void usb_init(void);
void uhci_init(void);
void ohci_init(void);
void ehci_init(void);
void xhci_init(void);

int uhci_probe(const struct pci_device *dev);
int ohci_probe(const struct pci_device *dev);
int ehci_probe(const struct pci_device *dev);
int xhci_probe(const struct pci_device *dev);

#endif /* _ASTRA_DRIVERS_USB_H */
