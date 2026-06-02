/*
 * AstraOS - PCI Bus Driver
 * PCI configuration-space access through legacy I/O ports 0xCF8/0xCFC.
 */

#include "pci.h"
#include "serial.h"
#include "../arch/x86_64/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_VENDOR_ID_NONE 0xFFFF
#define PCI_HEADER_TYPE_MULTI_FUNCTION 0x80

static uint32_t pci_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return 0x80000000U |
           ((uint32_t)bus << 16) |
           ((uint32_t)(device & 0x1F) << 11) |
           ((uint32_t)(function & 0x07) << 8) |
           (offset & 0xFC);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, device, function, offset);
    return (uint16_t)((value >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, device, function, offset);
    return (uint8_t)((value >> ((offset & 3) * 8)) & 0xFF);
}

static void pci_print_hex_digit(uint8_t value) {
    static const char hex[] = "0123456789ABCDEF";
    serial_putchar(hex[value & 0x0F]);
}

static void pci_print_hex8(uint8_t value) {
    pci_print_hex_digit(value >> 4);
    pci_print_hex_digit(value);
}

static void pci_print_hex16(uint16_t value) {
    pci_print_hex8((uint8_t)(value >> 8));
    pci_print_hex8((uint8_t)value);
}

static void pci_print_device(const struct pci_device *dev) {
    serial_puts("PCI ");
    pci_print_hex8(dev->bus);
    serial_putchar(':');
    pci_print_hex8(dev->device);
    serial_putchar('.');
    pci_print_hex_digit(dev->function);
    serial_puts(" vendor=0x");
    pci_print_hex16(dev->vendor_id);
    serial_puts(" device=0x");
    pci_print_hex16(dev->device_id);
    serial_puts(" class=0x");
    pci_print_hex8(dev->class_code);
    serial_puts(" subclass=0x");
    pci_print_hex8(dev->subclass);
    serial_puts(" prog_if=0x");
    pci_print_hex8(dev->prog_if);
    serial_puts("\n");
}

static void pci_probe_function(uint8_t bus, uint8_t device, uint8_t function, uint32_t *count) {
    uint16_t vendor_id = pci_config_read16(bus, device, function, 0x00);
    if (vendor_id == PCI_VENDOR_ID_NONE) {
        return;
    }

    struct pci_device dev;
    dev.bus = bus;
    dev.device = device;
    dev.function = function;
    dev.vendor_id = vendor_id;
    dev.device_id = pci_config_read16(bus, device, function, 0x02);
    dev.revision_id = pci_config_read8(bus, device, function, 0x08);
    dev.prog_if = pci_config_read8(bus, device, function, 0x09);
    dev.subclass = pci_config_read8(bus, device, function, 0x0A);
    dev.class_code = pci_config_read8(bus, device, function, 0x0B);
    dev.header_type = pci_config_read8(bus, device, function, 0x0E);

    pci_print_device(&dev);
    (*count)++;
}

static void pci_probe_device(uint8_t bus, uint8_t device, uint32_t *count) {
    uint16_t vendor_id = pci_config_read16(bus, device, 0, 0x00);
    if (vendor_id == PCI_VENDOR_ID_NONE) {
        return;
    }

    pci_probe_function(bus, device, 0, count);

    uint8_t header_type = pci_config_read8(bus, device, 0, 0x0E);
    if ((header_type & PCI_HEADER_TYPE_MULTI_FUNCTION) == 0) {
        return;
    }

    for (uint8_t function = 1; function < 8; function++) {
        pci_probe_function(bus, device, function, count);
    }
}

static void pci_print_dec(uint32_t value) {
    char buf[11];
    int pos = 0;

    if (value == 0) {
        serial_putchar('0');
        return;
    }

    while (value > 0 && pos < (int)sizeof(buf)) {
        buf[pos++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (pos > 0) {
        serial_putchar(buf[--pos]);
    }
}

void pci_init(void) {
    uint32_t count = 0;

    serial_puts("PCI: Enumerating buses\n");

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            pci_probe_device((uint8_t)bus, device, &count);
        }
    }

    serial_puts("PCI: Found ");
    pci_print_dec(count);
    serial_puts(" device(s)\n");
}
