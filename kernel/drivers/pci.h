/*
 * AstraOS - PCI Bus Driver Header
 * PCI configuration-space access and device enumeration
 */

#ifndef _ASTRA_DRIVERS_PCI_H
#define _ASTRA_DRIVERS_PCI_H

#include <stdint.h>

struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t header_type;
};

/*
 * pci_init - Enumerate PCI devices and print present devices to serial.
 */
void pci_init(void);

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

#endif /* _ASTRA_DRIVERS_PCI_H */
