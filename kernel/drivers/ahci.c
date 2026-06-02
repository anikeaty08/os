/*
 * AstraOS - AHCI PCI Probe Driver
 *
 * This driver is intentionally read-only: it enumerates PCI resources and
 * capabilities without touching the AHCI MMIO register block.
 */

#include "ahci.h"
#include "serial.h"

void ahci_init(void) {
    serial_puts("AHCI: registered read-only PCI probe; port reset, FIS, DMA, and disk I/O unsupported\n");
}

int ahci_probe(const struct pci_device *dev) {
    serial_puts("AHCI: probing ");
    pci_log_location(dev);
    serial_puts(" vendor=0x");
    pci_log_hex16(dev->vendor_id);
    serial_puts(" device=0x");
    pci_log_hex16(dev->device_id);
    serial_puts("\n");

    serial_puts("AHCI: ABAR is expected in BAR5; no MMIO registers will be read or written\n");
    pci_log_bars(dev, "AHCI: ");
    pci_log_capabilities(dev, "AHCI: ");
    serial_puts("AHCI: controller commands, port spin-up, identify, read, and write remain unsupported\n");

    return 0;
}

const struct pci_driver ahci_pci_driver = {
    .name = "AHCI",
    .class_code = 0x01,
    .class_mask = 0xFF,
    .subclass = 0x06,
    .subclass_mask = 0xFF,
    .prog_if = 0x01,
    .prog_if_mask = 0xFF,
    .probe = ahci_probe,
    .init = ahci_init,
};
