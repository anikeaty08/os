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

struct pci_bar {
    uint8_t index;
    uint8_t present;
    uint8_t is_io;
    uint8_t is_64;
    uint8_t prefetchable;
    uint32_t raw_low;
    uint32_t raw_high;
    uint64_t address;
};

struct pci_capability {
    uint8_t offset;
    uint8_t id;
    uint8_t next;
};

typedef void (*pci_capability_callback_t)(const struct pci_device *dev,
                                          const struct pci_capability *cap,
                                          void *ctx);

struct pci_driver {
    const char *name;
    uint8_t class_code;
    uint8_t class_mask;
    uint8_t subclass;
    uint8_t subclass_mask;
    uint8_t prog_if;
    uint8_t prog_if_mask;
    int (*probe)(const struct pci_device *dev);
    void (*init)(void);
};

struct pci_probe_summary {
    uint32_t devices;
    uint32_t driver_count;
    uint32_t ahci_matches;
    uint32_t nvme_matches;
    uint32_t usb_matches;
    uint32_t net_matches;
    uint32_t failures;
};

/*
 * pci_init - Enumerate PCI devices and print present devices to serial.
 */
void pci_init(void);
void pci_get_probe_summary(struct pci_probe_summary *summary);

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_config_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
const char *pci_class_name(uint8_t class_code, uint8_t subclass, uint8_t prog_if);

uint8_t pci_bar_count(const struct pci_device *dev);
int pci_read_bar(const struct pci_device *dev, uint8_t index, struct pci_bar *bar);
uint32_t pci_for_each_capability(const struct pci_device *dev,
                                 pci_capability_callback_t callback,
                                 void *ctx);
int pci_driver_matches(const struct pci_driver *driver, const struct pci_device *dev);

void pci_log_location(const struct pci_device *dev);
void pci_log_hex8(uint8_t value);
void pci_log_hex16(uint16_t value);
void pci_log_hex32(uint32_t value);
void pci_log_hex64(uint64_t value);
void pci_log_dec(uint32_t value);
void pci_log_bar(const struct pci_device *dev, uint8_t index, const char *prefix);
void pci_log_bars(const struct pci_device *dev, const char *prefix);
void pci_log_capabilities(const struct pci_device *dev, const char *prefix);

#endif /* _ASTRA_DRIVERS_PCI_H */
