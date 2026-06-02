/*
 * AstraOS - PCI Bus Driver
 * PCI configuration-space access through legacy I/O ports 0xCF8/0xCFC.
 */

#include "pci.h"
#include "ahci.h"
#include "nvme.h"
#include "serial.h"
#include "usb.h"
#include "net.h"
#include "../arch/x86_64/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_VENDOR_ID_NONE 0xFFFF
#define PCI_HEADER_TYPE_MULTI_FUNCTION 0x80
#define PCI_STATUS_CAPABILITIES 0x0010
#define PCI_CAPABILITY_POINTER 0x34

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

uint8_t pci_bar_count(const struct pci_device *dev) {
    if (!dev) {
        return 0;
    }

    switch (dev->header_type & 0x7F) {
        case 0x00:
            return 6;
        case 0x01:
            return 2;
        default:
            return 0;
    }
}

int pci_read_bar(const struct pci_device *dev, uint8_t index, struct pci_bar *bar) {
    if (!dev || !bar || index >= pci_bar_count(dev)) {
        return -1;
    }

    uint8_t offset = (uint8_t)(0x10 + (index * 4));
    uint32_t raw = pci_config_read32(dev->bus, dev->device, dev->function, offset);

    bar->index = index;
    bar->raw_low = raw;
    bar->raw_high = 0;
    bar->present = raw != 0 && raw != 0xFFFFFFFFU;
    bar->is_io = (uint8_t)(raw & 0x01);
    bar->is_64 = 0;
    bar->prefetchable = 0;
    bar->address = 0;

    if (!bar->present) {
        return 0;
    }

    if (bar->is_io) {
        bar->address = (uint64_t)(raw & 0xFFFFFFFCU);
        return 0;
    }

    uint8_t memory_type = (uint8_t)((raw >> 1) & 0x03);
    bar->prefetchable = (uint8_t)((raw >> 3) & 0x01);
    bar->address = (uint64_t)(raw & 0xFFFFFFF0U);

    if (memory_type == 0x02 && (index + 1) < pci_bar_count(dev)) {
        bar->is_64 = 1;
        bar->raw_high = pci_config_read32(dev->bus, dev->device, dev->function,
                                          (uint8_t)(offset + 4));
        bar->address |= ((uint64_t)bar->raw_high << 32);
    }

    return 0;
}

uint32_t pci_for_each_capability(const struct pci_device *dev,
                                 pci_capability_callback_t callback,
                                 void *ctx) {
    if (!dev) {
        return 0;
    }

    uint16_t status = pci_config_read16(dev->bus, dev->device, dev->function, 0x06);
    if ((status & PCI_STATUS_CAPABILITIES) == 0) {
        return 0;
    }

    uint8_t header_layout = (uint8_t)(dev->header_type & 0x7F);
    if (header_layout != 0x00 && header_layout != 0x01) {
        return 0;
    }

    uint8_t ptr = (uint8_t)(pci_config_read8(dev->bus, dev->device, dev->function,
                                             PCI_CAPABILITY_POINTER) & 0xFC);
    uint32_t count = 0;

    for (uint8_t guard = 0; guard < 48 && ptr >= 0x40; guard++) {
        struct pci_capability cap;
        cap.offset = ptr;
        cap.id = pci_config_read8(dev->bus, dev->device, dev->function, ptr);
        cap.next = pci_config_read8(dev->bus, dev->device, dev->function,
                                    (uint8_t)(ptr + 1));

        if (cap.id == 0x00 || cap.id == 0xFF) {
            break;
        }

        count++;
        if (callback) {
            callback(dev, &cap, ctx);
        }

        uint8_t next = (uint8_t)(cap.next & 0xFC);
        if (next == ptr) {
            break;
        }
        ptr = next;
    }

    return count;
}

int pci_driver_matches(const struct pci_driver *driver, const struct pci_device *dev) {
    if (!driver || !dev) {
        return 0;
    }

    return ((dev->class_code & driver->class_mask) ==
            (driver->class_code & driver->class_mask)) &&
           ((dev->subclass & driver->subclass_mask) ==
            (driver->subclass & driver->subclass_mask)) &&
           ((dev->prog_if & driver->prog_if_mask) ==
            (driver->prog_if & driver->prog_if_mask));
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

static void pci_log_hex_digit(uint8_t value) {
    static const char hex[] = "0123456789ABCDEF";
    serial_putchar(hex[value & 0x0F]);
}

void pci_log_hex8(uint8_t value) {
    pci_log_hex_digit(value >> 4);
    pci_log_hex_digit(value);
}

void pci_log_hex16(uint16_t value) {
    pci_log_hex8((uint8_t)(value >> 8));
    pci_log_hex8((uint8_t)value);
}

void pci_log_hex32(uint32_t value) {
    pci_log_hex16((uint16_t)(value >> 16));
    pci_log_hex16((uint16_t)value);
}

void pci_log_hex64(uint64_t value) {
    pci_log_hex32((uint32_t)(value >> 32));
    pci_log_hex32((uint32_t)value);
}

void pci_log_location(const struct pci_device *dev) {
    if (!dev) {
        return;
    }

    pci_log_hex8(dev->bus);
    serial_putchar(':');
    pci_log_hex8(dev->device);
    serial_putchar('.');
    pci_log_hex_digit(dev->function);
}

static void pci_print_device(const struct pci_device *dev) {
    serial_puts("PCI ");
    pci_log_location(dev);
    serial_puts(" vendor=0x");
    pci_log_hex16(dev->vendor_id);
    serial_puts(" device=0x");
    pci_log_hex16(dev->device_id);
    serial_puts(" class=0x");
    pci_log_hex8(dev->class_code);
    serial_puts(" subclass=0x");
    pci_log_hex8(dev->subclass);
    serial_puts(" prog_if=0x");
    pci_log_hex8(dev->prog_if);
    serial_puts(" name=\"");
    serial_puts(pci_class_name(dev->class_code, dev->subclass, dev->prog_if));
    serial_putchar('"');
    serial_puts("\n");
}

void pci_log_dec(uint32_t value) {
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

void pci_log_bar(const struct pci_device *dev, uint8_t index, const char *prefix) {
    struct pci_bar bar;
    if (pci_read_bar(dev, index, &bar) != 0) {
        return;
    }

    serial_puts(prefix);
    serial_puts("BAR");
    pci_log_dec(index);
    serial_puts(" raw=0x");
    pci_log_hex32(bar.raw_low);

    if (!bar.present) {
        serial_puts(" absent\n");
        return;
    }

    if (bar.is_io) {
        serial_puts(" io addr=0x");
        pci_log_hex64(bar.address);
        serial_puts("\n");
        return;
    }

    if (bar.is_64) {
        serial_puts(" high=0x");
        pci_log_hex32(bar.raw_high);
    }

    serial_puts(" mem");
    if (bar.is_64) {
        serial_puts("64");
    } else {
        serial_puts("32");
    }
    if (bar.prefetchable) {
        serial_puts(" prefetch");
    }
    serial_puts(" addr=0x");
    pci_log_hex64(bar.address);
    serial_puts("\n");
}

void pci_log_bars(const struct pci_device *dev, const char *prefix) {
    uint8_t count = pci_bar_count(dev);
    if (count == 0) {
        serial_puts(prefix);
        serial_puts("no endpoint BARs for this header type\n");
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        struct pci_bar bar;
        pci_log_bar(dev, i, prefix);
        if (pci_read_bar(dev, i, &bar) == 0 && bar.present && bar.is_64) {
            i++;
        }
    }
}

static void pci_log_capability_callback(const struct pci_device *dev,
                                        const struct pci_capability *cap,
                                        void *ctx) {
    const char *prefix = (const char *)ctx;
    (void)dev;

    serial_puts(prefix);
    serial_puts("cap offset=0x");
    pci_log_hex8(cap->offset);
    serial_puts(" id=0x");
    pci_log_hex8(cap->id);
    serial_puts(" next=0x");
    pci_log_hex8((uint8_t)(cap->next & 0xFC));
    serial_puts("\n");
}

void pci_log_capabilities(const struct pci_device *dev, const char *prefix) {
    uint32_t count = pci_for_each_capability(dev, pci_log_capability_callback,
                                             (void *)prefix);
    if (count == 0) {
        serial_puts(prefix);
        serial_puts("no standard PCI capabilities reported\n");
    }
}

struct pci_driver_probe_stat {
    const struct pci_driver *driver;
    uint32_t matches;
    uint32_t failures;
};

static const struct pci_driver *pci_builtin_drivers[] = {
    &ahci_pci_driver,
    &nvme_pci_driver,
    &uhci_pci_driver,
    &ohci_pci_driver,
    &ehci_pci_driver,
    &xhci_pci_driver,
    &net_pci_driver,
};

#define PCI_BUILTIN_DRIVER_COUNT \
    ((uint32_t)(sizeof(pci_builtin_drivers) / sizeof(pci_builtin_drivers[0])))

static void pci_init_builtin_drivers(void) {
    for (uint32_t i = 0; i < PCI_BUILTIN_DRIVER_COUNT; i++) {
        if (pci_builtin_drivers[i]->init) {
            pci_builtin_drivers[i]->init();
        }
    }
}

static void pci_dispatch_driver_probes(const struct pci_device *dev,
                                       struct pci_driver_probe_stat *stats,
                                       uint32_t stats_count) {
    for (uint32_t i = 0; i < stats_count; i++) {
        const struct pci_driver *driver = stats[i].driver;
        if (!pci_driver_matches(driver, dev)) {
            continue;
        }

        stats[i].matches++;
        if (!driver->probe) {
            serial_puts("PCI: ");
            serial_puts(driver->name);
            serial_puts(" matched ");
            pci_log_location(dev);
            serial_puts(" but has no probe function\n");
            stats[i].failures++;
            continue;
        }

        int result = driver->probe(dev);
        if (result != 0) {
            serial_puts("PCI: ");
            serial_puts(driver->name);
            serial_puts(" probe failed for ");
            pci_log_location(dev);
            serial_puts(" rc=");
            pci_log_dec((uint32_t)(0 - result));
            serial_puts("\n");
            stats[i].failures++;
        }
    }
}

static void pci_probe_function(uint8_t bus,
                               uint8_t device,
                               uint8_t function,
                               uint32_t *count,
                               struct pci_driver_probe_stat *stats,
                               uint32_t stats_count) {
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
    pci_dispatch_driver_probes(&dev, stats, stats_count);
    (*count)++;
}

void pci_init(void) {
    uint32_t count = 0;
    struct pci_driver_probe_stat stats[PCI_BUILTIN_DRIVER_COUNT];

    serial_puts("PCI: Enumerating buses\n");
    pci_init_builtin_drivers();

    for (uint32_t i = 0; i < PCI_BUILTIN_DRIVER_COUNT; i++) {
        stats[i].driver = pci_builtin_drivers[i];
        stats[i].matches = 0;
        stats[i].failures = 0;
    }

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            uint16_t vendor_id = pci_config_read16((uint8_t)bus, device, 0, 0x00);
            if (vendor_id == PCI_VENDOR_ID_NONE) {
                continue;
            }

            pci_probe_function((uint8_t)bus, device, 0, &count, stats,
                               PCI_BUILTIN_DRIVER_COUNT);

            uint8_t header_type = pci_config_read8((uint8_t)bus, device, 0, 0x0E);
            if ((header_type & PCI_HEADER_TYPE_MULTI_FUNCTION) == 0) {
                continue;
            }

            for (uint8_t function = 1; function < 8; function++) {
                pci_probe_function((uint8_t)bus, device, function, &count, stats,
                                   PCI_BUILTIN_DRIVER_COUNT);
            }
        }
    }

    serial_puts("PCI: Found ");
    pci_log_dec(count);
    serial_puts(" device(s)\n");

    serial_puts("PCI: Driver probe matches");
    for (uint32_t i = 0; i < PCI_BUILTIN_DRIVER_COUNT; i++) {
        serial_putchar(' ');
        serial_puts(stats[i].driver->name);
        serial_putchar('=');
        pci_log_dec(stats[i].matches);
        if (stats[i].failures != 0) {
            serial_puts("(fail=");
            pci_log_dec(stats[i].failures);
            serial_putchar(')');
        }
    }
    serial_puts("\n");
}
