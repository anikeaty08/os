/*
 * AstraOS - USB PCI Probe Driver
 *
 * Supports UHCI/OHCI/EHCI/XHCI detection at PCI probe level only.
 */

#include "usb.h"
#include "serial.h"

static const char *usb_controller_name(uint8_t prog_if) {
    switch (prog_if) {
        case 0x00:
            return "UHCI";
        case 0x10:
            return "OHCI";
        case 0x20:
            return "EHCI";
        case 0x30:
            return "XHCI";
        default:
            return "unknown USB";
    }
}

static uint8_t usb_expected_bar(uint8_t prog_if) {
    if (prog_if == 0x00) {
        return 4;
    }
    return 0;
}

void usb_init(void) {
    serial_puts("USB: registered read-only PCI probe; UHCI/OHCI/EHCI/XHCI scheduling and transfers unsupported\n");
}

void uhci_init(void) {
    serial_puts("UHCI: registered read-only PCI probe; frame lists, port reset, and transfers unsupported\n");
}

void ohci_init(void) {
    serial_puts("OHCI: registered read-only PCI probe; HCCA, endpoint lists, and transfers unsupported\n");
}

void ehci_init(void) {
    serial_puts("EHCI: registered read-only PCI probe; async/periodic schedules and transfers unsupported\n");
}

void xhci_init(void) {
    serial_puts("XHCI: registered read-only PCI probe; command rings, event rings, and transfers unsupported\n");
}

static int usb_probe_common(const struct pci_device *dev) {
    const char *name = usb_controller_name(dev->prog_if);

    serial_puts("USB: probing ");
    pci_log_location(dev);
    serial_puts(" type=");
    serial_puts(name);
    serial_puts(" vendor=0x");
    pci_log_hex16(dev->vendor_id);
    serial_puts(" device=0x");
    pci_log_hex16(dev->device_id);
    serial_puts(" prog_if=0x");
    pci_log_hex8(dev->prog_if);
    serial_puts("\n");

    if (dev->prog_if != 0x00 && dev->prog_if != 0x10 &&
        dev->prog_if != 0x20 && dev->prog_if != 0x30) {
        serial_puts("USB: unsupported USB programming interface; diagnostics only\n");
    }

    serial_puts("USB: primary controller BAR expected at BAR");
    pci_log_dec(usb_expected_bar(dev->prog_if));
    serial_puts("; no host-controller registers will be read or written\n");
    pci_log_bars(dev, "USB: ");
    pci_log_capabilities(dev, "USB: ");
    serial_puts("USB: port reset, device enumeration, transfer rings, schedules, and interrupts remain unsupported\n");

    return 0;
}

int uhci_probe(const struct pci_device *dev) {
    return usb_probe_common(dev);
}

int ohci_probe(const struct pci_device *dev) {
    return usb_probe_common(dev);
}

int ehci_probe(const struct pci_device *dev) {
    return usb_probe_common(dev);
}

int xhci_probe(const struct pci_device *dev) {
    return usb_probe_common(dev);
}

const struct pci_driver uhci_pci_driver = {
    .name = "UHCI",
    .class_code = 0x0C,
    .class_mask = 0xFF,
    .subclass = 0x03,
    .subclass_mask = 0xFF,
    .prog_if = 0x00,
    .prog_if_mask = 0xFF,
    .probe = uhci_probe,
    .init = uhci_init,
};

const struct pci_driver ohci_pci_driver = {
    .name = "OHCI",
    .class_code = 0x0C,
    .class_mask = 0xFF,
    .subclass = 0x03,
    .subclass_mask = 0xFF,
    .prog_if = 0x10,
    .prog_if_mask = 0xFF,
    .probe = ohci_probe,
    .init = ohci_init,
};

const struct pci_driver ehci_pci_driver = {
    .name = "EHCI",
    .class_code = 0x0C,
    .class_mask = 0xFF,
    .subclass = 0x03,
    .subclass_mask = 0xFF,
    .prog_if = 0x20,
    .prog_if_mask = 0xFF,
    .probe = ehci_probe,
    .init = ehci_init,
};

const struct pci_driver xhci_pci_driver = {
    .name = "XHCI",
    .class_code = 0x0C,
    .class_mask = 0xFF,
    .subclass = 0x03,
    .subclass_mask = 0xFF,
    .prog_if = 0x30,
    .prog_if_mask = 0xFF,
    .probe = xhci_probe,
    .init = xhci_init,
};
