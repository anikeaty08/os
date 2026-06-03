/*
 * AstraOS - Network Driver
 * Minimal RTL8139 raw Ethernet packet I/O.
 */

#include "net.h"
#include "serial.h"
#include "../arch/x86_64/io.h"
#include "../lib/string.h"
#include "../mm/pmm.h"

#define NET_MAX_DEVICES             4

#define PCI_COMMAND                 0x04
#define PCI_COMMAND_IO              (1U << 0)
#define PCI_COMMAND_MEMORY          (1U << 1)
#define PCI_COMMAND_BUS_MASTER      (1U << 2)

#define RTL_VENDOR                  0x10EC
#define RTL_DEVICE_8139             0x8139

#define RTL_IDR0                    0x00
#define RTL_TSD0                    0x10
#define RTL_TSAD0                   0x20
#define RTL_RBSTART                 0x30
#define RTL_CR                      0x37
#define RTL_CAPR                    0x38
#define RTL_IMR                     0x3C
#define RTL_ISR                     0x3E
#define RTL_TCR                     0x40
#define RTL_RCR                     0x44
#define RTL_CONFIG1                 0x52

#define RTL_CR_BUFE                 0x01
#define RTL_CR_TE                   0x04
#define RTL_CR_RE                   0x08
#define RTL_CR_RST                  0x10

#define RTL_ISR_ROK                 0x0001
#define RTL_ISR_RER                 0x0002
#define RTL_ISR_TOK                 0x0004
#define RTL_ISR_TER                 0x0008

#define RTL_TSD_TOK                 (1U << 15)
#define RTL_TSD_TUN                 (1U << 14)
#define RTL_TX_COUNT                4
#define RTL_TX_BUF_SIZE             2048
#define RTL_RX_RING_SIZE            8192
#define RTL_RX_ALLOC_SIZE           (RTL_RX_RING_SIZE + 16 + 1500)
#define RTL_MIN_FRAME               60

extern uint64_t hhdm_offset;

struct net_device {
    bool present;
    uint16_t io_base;
    uint8_t mac[6];
    uint64_t rx_phys;
    uint8_t *rx;
    uint32_t rx_offset;
    uint64_t tx_phys[RTL_TX_COUNT];
    uint8_t *tx[RTL_TX_COUNT];
    uint8_t tx_index;
};

static struct net_device devices[NET_MAX_DEVICES];
static int device_count;

static void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static uint8_t rtl_inb(struct net_device *dev, uint16_t reg) {
    return inb((uint16_t)(dev->io_base + reg));
}

static uint16_t rtl_inw(struct net_device *dev, uint16_t reg) {
    return inw((uint16_t)(dev->io_base + reg));
}

static uint32_t rtl_inl(struct net_device *dev, uint16_t reg) {
    return inl((uint16_t)(dev->io_base + reg));
}

static void rtl_outb(struct net_device *dev, uint16_t reg, uint8_t value) {
    outb((uint16_t)(dev->io_base + reg), value);
}

static void rtl_outw(struct net_device *dev, uint16_t reg, uint16_t value) {
    outw((uint16_t)(dev->io_base + reg), value);
}

static void rtl_outl(struct net_device *dev, uint16_t reg, uint32_t value) {
    outl((uint16_t)(dev->io_base + reg), value);
}

static uint64_t alloc_pages_zero(size_t bytes, void **virt_out) {
    size_t pages = PAGE_ALIGN_UP(bytes) / PAGE_SIZE;
    void *phys = pmm_alloc_pages(pages);
    if (!phys) {
        return 0;
    }
    void *virt = phys_to_virt((uint64_t)phys);
    memset(virt, 0, pages * PAGE_SIZE);
    *virt_out = virt;
    return (uint64_t)phys;
}

static bool rtl8139_alloc_buffers(struct net_device *dev) {
    void *rx_virt = NULL;
    dev->rx_phys = alloc_pages_zero(RTL_RX_ALLOC_SIZE, &rx_virt);
    if (!dev->rx_phys) {
        return false;
    }
    dev->rx = (uint8_t *)rx_virt;

    for (uint32_t i = 0; i < RTL_TX_COUNT; i++) {
        void *tx_virt = NULL;
        dev->tx_phys[i] = alloc_pages_zero(RTL_TX_BUF_SIZE, &tx_virt);
        if (!dev->tx_phys[i]) {
            return false;
        }
        dev->tx[i] = (uint8_t *)tx_virt;
    }

    return true;
}

static bool rtl8139_reset(struct net_device *dev) {
    rtl_outb(dev, RTL_CONFIG1, 0x00);
    rtl_outb(dev, RTL_CR, RTL_CR_RST);
    for (uint32_t i = 0; i < 100000; i++) {
        if ((rtl_inb(dev, RTL_CR) & RTL_CR_RST) == 0) {
            return true;
        }
    }
    return false;
}

static bool rtl8139_init_device(struct net_device *dev) {
    if (!rtl8139_alloc_buffers(dev) || !rtl8139_reset(dev)) {
        return false;
    }

    for (uint32_t i = 0; i < 6; i++) {
        dev->mac[i] = rtl_inb(dev, (uint16_t)(RTL_IDR0 + i));
    }

    rtl_outl(dev, RTL_RBSTART, (uint32_t)dev->rx_phys);
    for (uint32_t i = 0; i < RTL_TX_COUNT; i++) {
        rtl_outl(dev, (uint16_t)(RTL_TSAD0 + i * 4), (uint32_t)dev->tx_phys[i]);
    }

    rtl_outw(dev, RTL_IMR, 0x0000);
    rtl_outw(dev, RTL_ISR, 0xFFFF);
    rtl_outl(dev, RTL_TCR, 0x03000700);
    rtl_outl(dev, RTL_RCR, 0x0000078F);
    rtl_outb(dev, RTL_CR, RTL_CR_RE | RTL_CR_TE);
    dev->rx_offset = 0;
    dev->tx_index = 0;
    return true;
}

void net_init(void) {
    device_count = 0;
    memset(devices, 0, sizeof(devices));
    serial_puts("NET: registered RTL8139 raw Ethernet driver\n");
}

int net_probe(const struct pci_device *dev) {
    if (device_count >= NET_MAX_DEVICES) {
        return -1;
    }

    if (dev->vendor_id != RTL_VENDOR || dev->device_id != RTL_DEVICE_8139) {
        serial_puts("NET: Ethernet controller ");
        pci_log_location(dev);
        serial_puts(" is not RTL8139; raw packet driver unavailable\n");
        return 0;
    }

    struct pci_bar bar;
    if (pci_read_bar(dev, 0, &bar) != 0 || !bar.present || !bar.is_io) {
        serial_puts("NET: RTL8139 BAR0 I/O base missing\n");
        return -1;
    }

    uint16_t command = pci_config_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER;
    pci_config_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, command);

    struct net_device *netdev = &devices[device_count];
    memset(netdev, 0, sizeof(*netdev));
    netdev->io_base = (uint16_t)bar.address;

    serial_puts("NET: probing RTL8139 ");
    pci_log_location(dev);
    serial_puts(" io=0x");
    pci_log_hex16(netdev->io_base);
    serial_puts("\n");

    if (!rtl8139_init_device(netdev)) {
        serial_puts("NET: RTL8139 initialization failed\n");
        return -1;
    }

    netdev->present = true;
    serial_puts("NET: RTL8139 ready mac=");
    for (uint32_t i = 0; i < 6; i++) {
        if (i) serial_putchar(':');
        pci_log_hex8(netdev->mac[i]);
    }
    serial_puts("\n");

    device_count++;
    return 0;
}

int net_device_count(void) {
    return device_count;
}

bool net_device_present(int index) {
    return index >= 0 && index < device_count && devices[index].present;
}

const uint8_t *net_get_mac(int index) {
    if (!net_device_present(index)) {
        return NULL;
    }
    return devices[index].mac;
}

int net_send(int index, const void *frame, uint16_t length) {
    if (!net_device_present(index) || !frame || length > RTL_TX_BUF_SIZE) {
        return -1;
    }

    struct net_device *dev = &devices[index];
    uint8_t slot = dev->tx_index;
    uint16_t send_len = length < RTL_MIN_FRAME ? RTL_MIN_FRAME : length;

    memset(dev->tx[slot], 0, RTL_TX_BUF_SIZE);
    memcpy(dev->tx[slot], frame, length);
    rtl_outl(dev, (uint16_t)(RTL_TSD0 + slot * 4), send_len);

    for (uint32_t spin = 0; spin < 1000000; spin++) {
        uint32_t tsd = rtl_inl(dev, (uint16_t)(RTL_TSD0 + slot * 4));
        if (tsd & RTL_TSD_TOK) {
            dev->tx_index = (uint8_t)((slot + 1) % RTL_TX_COUNT);
            rtl_outw(dev, RTL_ISR, RTL_ISR_TOK);
            return length;
        }
        if (tsd & RTL_TSD_TUN) {
            break;
        }
    }

    rtl_outw(dev, RTL_ISR, RTL_ISR_TER);
    return -1;
}

int net_poll(int index, void *buffer, uint16_t max_length) {
    if (!net_device_present(index) || !buffer || max_length == 0) {
        return -1;
    }

    struct net_device *dev = &devices[index];
    if (rtl_inb(dev, RTL_CR) & RTL_CR_BUFE) {
        return 0;
    }

    uint32_t offset = dev->rx_offset % RTL_RX_RING_SIZE;
    uint8_t *packet = dev->rx + offset;
    uint16_t status = *(uint16_t *)(packet + 0);
    uint16_t length = *(uint16_t *)(packet + 2);

    if ((status & 0x0001) == 0 || length < 4 || length > RTL_RX_ALLOC_SIZE) {
        rtl_outw(dev, RTL_ISR, RTL_ISR_RER);
        dev->rx_offset = (dev->rx_offset + 4) % RTL_RX_RING_SIZE;
        rtl_outw(dev, RTL_CAPR, (uint16_t)(dev->rx_offset - 16));
        return -1;
    }

    uint16_t payload_len = (uint16_t)(length - 4);
    if (payload_len > max_length) {
        payload_len = max_length;
    }

    uint32_t data_offset = (offset + 4) % RTL_RX_RING_SIZE;
    uint8_t *out = (uint8_t *)buffer;
    for (uint32_t i = 0; i < payload_len; i++) {
        out[i] = dev->rx[(data_offset + i) % RTL_RX_RING_SIZE];
    }

    dev->rx_offset = (dev->rx_offset + length + 4 + 3) & ~3U;
    dev->rx_offset %= RTL_RX_RING_SIZE;
    rtl_outw(dev, RTL_CAPR, (uint16_t)(dev->rx_offset - 16));
    rtl_outw(dev, RTL_ISR, RTL_ISR_ROK);
    return payload_len;
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
