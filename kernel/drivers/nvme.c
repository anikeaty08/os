/*
 * AstraOS - NVMe PCI Probe Driver
 *
 * This module only inspects PCI configuration space. It does not enable bus
 * mastering, map controller MMIO, or submit admin commands.
 */

#include "nvme.h"
#include "serial.h"

void nvme_init(void) {
    serial_puts("NVMe: registered read-only PCI probe; admin queue setup and block I/O unsupported\n");
}

int nvme_probe(const struct pci_device *dev) {
    serial_puts("NVMe: probing ");
    pci_log_location(dev);
    serial_puts(" vendor=0x");
    pci_log_hex16(dev->vendor_id);
    serial_puts(" device=0x");
    pci_log_hex16(dev->device_id);
    serial_puts(" prog_if=0x");
    pci_log_hex8(dev->prog_if);
    serial_puts("\n");

    if (dev->prog_if != 0x02) {
        serial_puts("NVMe: non-standard NVM programming interface; treating as unsupported probe-only device\n");
    }

    serial_puts("NVMe: controller MMIO base is expected in BAR0/BAR1; no MMIO registers will be read or written\n");
    pci_log_bars(dev, "NVMe: ");
    pci_log_capabilities(dev, "NVMe: ");
    serial_puts("NVMe: reset, controller enable, queue creation, identify, read, and write remain unsupported\n");

    return 0;
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
