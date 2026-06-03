/*
 * AstraOS - USB PCI Probe Driver Header
 */

#ifndef _ASTRA_DRIVERS_USB_H
#define _ASTRA_DRIVERS_USB_H

#include <stdbool.h>
#include <stdint.h>
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
int usb_controller_count(void);
int usb_port_count(int controller);
bool usb_port_connected(int controller, int port);
bool usb_port_enabled(int controller, int port);
int usb_device_count(void);
const char *usb_device_class_name(int index);
bool usb_device_is_hid(int index);
bool usb_device_is_mass_storage(int index);
uint16_t usb_device_vendor(int index);
uint16_t usb_device_product(int index);

#endif /* _ASTRA_DRIVERS_USB_H */
