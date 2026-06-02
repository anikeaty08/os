/*
 * AstraOS - Network PCI Probe Driver
 *
 * This driver detects PCI network controllers and records their resources, but
 * does not configure MACs, DMA rings, interrupts, or packet paths.
 */

#include "net.h"
#include "serial.h"

static const char *net_controller_name(uint8_t subclass) {
    switch (subclass) {
        case 0x00:
            return "Ethernet";
        case 0x01:
            return "Token Ring";
        case 0x02:
            return "FDDI";
        case 0x03:
            return "ATM";
        case 0x04:
            return "ISDN";
        case 0x07:
            return "InfiniBand";
        default:
            return "network";
    }
}

void net_init(void) {
    serial_puts("NET: registered read-only PCI probe; link setup, RX/TX DMA, and packet I/O unsupported\n");
}

int net_probe(const struct pci_device *dev) {
    serial_puts("NET: probing ");
    pci_log_location(dev);
    serial_puts(" type=");
    serial_puts(net_controller_name(dev->subclass));
    serial_puts(" vendor=0x");
    pci_log_hex16(dev->vendor_id);
    serial_puts(" device=0x");
    pci_log_hex16(dev->device_id);
    serial_puts(" subclass=0x");
    pci_log_hex8(dev->subclass);
    serial_puts("\n");

    if (dev->subclass != 0x00) {
        serial_puts("NET: non-Ethernet network controller; diagnostics only\n");
    }

    pci_log_bars(dev, "NET: ");
    pci_log_capabilities(dev, "NET: ");
    serial_puts("NET: MAC programming, PHY/link management, RX/TX rings, interrupts, and packets remain unsupported\n");

    return 0;
}

const struct pci_driver net_pci_driver = {
    .name = "NET",
    .class_code = 0x02,
    .class_mask = 0xFF,
    .subclass = 0x00,
    .subclass_mask = 0x00,
    .prog_if = 0x00,
    .prog_if_mask = 0x00,
    .probe = net_probe,
    .init = net_init,
};
