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

const char *pci_class_name(uint8_t class_code, uint8_t subclass, uint8_t prog_if) {
    switch (class_code) {
        case 0x00:
            if (subclass == 0x00) {
                return "Unclassified non-VGA device";
            }
            if (subclass == 0x01) {
                return "Unclassified VGA-compatible device";
            }
            return "Unclassified device";
        case 0x01:
            switch (subclass) {
                case 0x00:
                    return "SCSI storage controller";
                case 0x01:
                    return "IDE storage controller";
                case 0x02:
                    return "Floppy disk controller";
                case 0x03:
                    return "IPI storage controller";
                case 0x04:
                    return "RAID storage controller";
                case 0x05:
                    return "ATA storage controller";
                case 0x06:
                    return "SATA storage controller";
                case 0x07:
                    return "Serial Attached SCSI controller";
                case 0x08:
                    return "Non-Volatile Memory controller";
                default:
                    return "Mass storage controller";
            }
        case 0x02:
            switch (subclass) {
                case 0x00:
                    return "Ethernet controller";
                case 0x01:
                    return "Token Ring controller";
                case 0x02:
                    return "FDDI controller";
                case 0x03:
                    return "ATM controller";
                case 0x04:
                    return "ISDN controller";
                case 0x05:
                    return "WorldFip controller";
                case 0x06:
                    return "PICMG controller";
                case 0x07:
                    return "InfiniBand controller";
                case 0x08:
                    return "Fabric controller";
                default:
                    return "Network controller";
            }
        case 0x03:
            switch (subclass) {
                case 0x00:
                    if (prog_if == 0x01) {
                        return "8514-compatible display controller";
                    }
                    return "VGA-compatible display controller";
                case 0x01:
                    return "XGA display controller";
                case 0x02:
                    return "3D display controller";
                default:
                    return "Display controller";
            }
        case 0x04:
            switch (subclass) {
                case 0x00:
                    return "Video device";
                case 0x01:
                    return "Audio device";
                case 0x02:
                    return "Computer telephony device";
                case 0x03:
                    return "High Definition Audio controller";
                default:
                    return "Multimedia controller";
            }
        case 0x05:
            switch (subclass) {
                case 0x00:
                    return "RAM memory controller";
                case 0x01:
                    return "Flash memory controller";
                default:
                    return "Memory controller";
            }
        case 0x06:
            switch (subclass) {
                case 0x00:
                    return "Host bridge";
                case 0x01:
                    return "ISA bridge";
                case 0x02:
                    return "EISA bridge";
                case 0x03:
                    return "MCA bridge";
                case 0x04:
                    return "PCI-to-PCI bridge";
                case 0x05:
                    return "PCMCIA bridge";
                case 0x06:
                    return "NuBus bridge";
                case 0x07:
                    return "CardBus bridge";
                case 0x08:
                    return "RACEway bridge";
                case 0x09:
                    return "PCI-to-PCI semi-transparent bridge";
                case 0x0A:
                    return "InfiniBand-to-PCI host bridge";
                default:
                    return "Bridge device";
            }
        case 0x07:
            switch (subclass) {
                case 0x00:
                    return "Serial controller";
                case 0x01:
                    return "Parallel controller";
                case 0x02:
                    return "Multiport serial controller";
                case 0x03:
                    return "Modem";
                case 0x04:
                    return "IEEE 488 controller";
                case 0x05:
                    return "Smart card controller";
                default:
                    return "Communication controller";
            }
        case 0x08:
            switch (subclass) {
                case 0x00:
                    return "Programmable interrupt controller";
                case 0x01:
                    return "DMA controller";
                case 0x02:
                    return "Timer";
                case 0x03:
                    return "RTC controller";
                case 0x04:
                    return "PCI hot-plug controller";
                case 0x05:
                    return "SD host controller";
                case 0x06:
                    return "IOMMU";
                default:
                    return "System peripheral";
            }
        case 0x09:
            switch (subclass) {
                case 0x00:
                    return "Keyboard controller";
                case 0x01:
                    return "Digitizer pen";
                case 0x02:
                    return "Mouse controller";
                case 0x03:
                    return "Scanner controller";
                case 0x04:
                    return "Gameport controller";
                default:
                    return "Input device controller";
            }
        case 0x0A:
            return "Docking station";
        case 0x0B:
            switch (subclass) {
                case 0x00:
                    return "386 processor";
                case 0x01:
                    return "486 processor";
                case 0x02:
                    return "Pentium processor";
                case 0x10:
                    return "Alpha processor";
                case 0x20:
                    return "PowerPC processor";
                case 0x30:
                    return "MIPS processor";
                case 0x40:
                    return "Co-processor";
                default:
                    return "Processor";
            }
        case 0x0C:
            switch (subclass) {
                case 0x00:
                    return "FireWire controller";
                case 0x01:
                    return "ACCESS.bus controller";
                case 0x02:
                    return "SSA controller";
                case 0x03:
                    return "USB controller";
                case 0x04:
                    return "Fibre Channel controller";
                case 0x05:
                    return "SMBus controller";
                case 0x06:
                    return "InfiniBand controller";
                case 0x07:
                    return "IPMI interface";
                case 0x08:
                    return "SERCOS interface";
                case 0x09:
                    return "CANbus controller";
                default:
                    return "Serial bus controller";
            }
        case 0x0D:
            return "Wireless controller";
        case 0x0E:
            return "Intelligent controller";
        case 0x0F:
            return "Satellite communication controller";
        case 0x10:
            return "Encryption controller";
        case 0x11:
            return "Signal processing controller";
        case 0x12:
            return "Processing accelerator";
        case 0x13:
            return "Non-essential instrumentation";
        case 0x40:
            return "Co-processor";
        case 0xFF:
            return "Unassigned class";
        default:
            return "Unknown PCI class";
    }
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
    serial_puts(" name=\"");
    serial_puts(pci_class_name(dev->class_code, dev->subclass, dev->prog_if));
    serial_putchar('"');
    serial_puts("\n");
}

static void pci_update_driver_gap_counters(const struct pci_device *dev,
                                           uint32_t *ahci,
                                           uint32_t *nvme,
                                           uint32_t *usb,
                                           uint32_t *network) {
    if (!dev) return;

    if (dev->class_code == 0x01 && dev->subclass == 0x06 && dev->prog_if == 0x01) {
        (*ahci)++;
    } else if (dev->class_code == 0x01 && dev->subclass == 0x08) {
        (*nvme)++;
    } else if (dev->class_code == 0x0C && dev->subclass == 0x03) {
        (*usb)++;
    } else if (dev->class_code == 0x02) {
        (*network)++;
    }
}

static void pci_probe_function_with_gaps(uint8_t bus,
                                         uint8_t device,
                                         uint8_t function,
                                         uint32_t *count,
                                         uint32_t *ahci,
                                         uint32_t *nvme,
                                         uint32_t *usb,
                                         uint32_t *network) {
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
    pci_update_driver_gap_counters(&dev, ahci, nvme, usb, network);
    (*count)++;
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
    uint32_t ahci_count = 0;
    uint32_t nvme_count = 0;
    uint32_t usb_count = 0;
    uint32_t network_count = 0;

    serial_puts("PCI: Enumerating buses\n");

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            uint16_t vendor_id = pci_config_read16((uint8_t)bus, device, 0, 0x00);
            if (vendor_id == PCI_VENDOR_ID_NONE) {
                continue;
            }

            pci_probe_function_with_gaps((uint8_t)bus, device, 0, &count,
                                         &ahci_count, &nvme_count,
                                         &usb_count, &network_count);

            uint8_t header_type = pci_config_read8((uint8_t)bus, device, 0, 0x0E);
            if ((header_type & PCI_HEADER_TYPE_MULTI_FUNCTION) == 0) {
                continue;
            }

            for (uint8_t function = 1; function < 8; function++) {
                pci_probe_function_with_gaps((uint8_t)bus, device, function, &count,
                                             &ahci_count, &nvme_count,
                                             &usb_count, &network_count);
            }
        }
    }

    serial_puts("PCI: Found ");
    pci_print_dec(count);
    serial_puts(" device(s)\n");
    serial_puts("PCI: Driver gaps AHCI=");
    pci_print_dec(ahci_count);
    serial_puts(" NVMe=");
    pci_print_dec(nvme_count);
    serial_puts(" USB=");
    pci_print_dec(usb_count);
    serial_puts(" network=");
    pci_print_dec(network_count);
    serial_puts("\n");
}
