/*
 * AstraOS - USB Host Controller Driver
 * UHCI controller initialization and root-port management.
 */

#include "usb.h"
#include "serial.h"
#include "../arch/x86_64/io.h"
#include "../lib/string.h"
#include "../mm/pmm.h"

#define USB_MAX_CONTROLLERS         8
#define USB_MAX_DEVICES             16
#define UHCI_FRAME_COUNT            1024
#define UHCI_MAX_TDS                32
#define USB_MAX_CONTROL_BYTES       256

#define PCI_COMMAND                 0x04
#define PCI_COMMAND_IO              (1U << 0)
#define PCI_COMMAND_BUS_MASTER      (1U << 2)

#define UHCI_USBCMD                 0x00
#define UHCI_USBSTS                 0x02
#define UHCI_USBINTR                0x04
#define UHCI_FRNUM                  0x06
#define UHCI_FLBASEADD              0x08
#define UHCI_SOFMOD                 0x0C
#define UHCI_PORTSC1                0x10
#define UHCI_PORTSC2                0x12

#define UHCI_CMD_RS                 (1U << 0)
#define UHCI_CMD_HCRESET            (1U << 1)
#define UHCI_CMD_CF                 (1U << 6)
#define UHCI_STS_HALTED             (1U << 5)
#define UHCI_PORT_CCS               (1U << 0)
#define UHCI_PORT_CSC               (1U << 1)
#define UHCI_PORT_PE                (1U << 2)
#define UHCI_PORT_PEC               (1U << 3)
#define UHCI_PORT_LSDA              (1U << 8)
#define UHCI_PORT_RESET             (1U << 9)

#define UHCI_LINK_TERMINATE         0x00000001U
#define UHCI_LINK_QH                0x00000002U
#define UHCI_TD_STATUS_ACTIVE       (1U << 23)
#define UHCI_TD_STATUS_IOC          (1U << 24)
#define UHCI_TD_STATUS_LS           (1U << 26)
#define UHCI_TD_STATUS_CERR         (3U << 27)
#define UHCI_TD_ERROR_MASK          0x007F0000U

#define USB_PID_OUT                 0xE1
#define USB_PID_IN                  0x69
#define USB_PID_SETUP               0x2D

#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_SET_IDLE            0x0A
#define USB_REQ_SET_PROTOCOL        0x0B

#define USB_DT_DEVICE               0x01
#define USB_DT_CONFIGURATION        0x02
#define USB_DT_INTERFACE            0x04
#define USB_DT_ENDPOINT             0x05

#define USB_CLASS_HID               0x03
#define USB_CLASS_MASS_STORAGE      0x08

#define USB_DIR_IN                  0x80
#define USB_ENDPOINT_NUMBER_MASK    0x0F
#define USB_ENDPOINT_TRANSFER_MASK  0x03
#define USB_ENDPOINT_INTERRUPT      0x03
#define USB_ENDPOINT_BULK           0x02

#define USB_MSC_CBW_SIGNATURE       0x43425355U
#define USB_MSC_CSW_SIGNATURE       0x53425355U
#define USB_MSC_CBW_TAG             0x41535452U

extern uint64_t hhdm_offset;

enum usb_controller_type {
    USB_CTRL_UHCI,
    USB_CTRL_OHCI,
    USB_CTRL_EHCI,
    USB_CTRL_XHCI,
};

struct usb_controller {
    bool present;
    enum usb_controller_type type;
    uint16_t io_base;
    uint8_t port_count;
    uint64_t frame_list_phys;
    uint32_t *frame_list;
};

struct uhci_qh {
    volatile uint32_t head;
    volatile uint32_t element;
};

struct uhci_td {
    volatile uint32_t link;
    volatile uint32_t status;
    volatile uint32_t token;
    volatile uint32_t buffer;
};

struct uhci_transfer_page {
    struct uhci_qh qh;
    struct uhci_td td[UHCI_MAX_TDS];
};

struct usb_device {
    bool present;
    bool low_speed;
    bool hid_ready;
    bool mass_ready;
    uint8_t controller;
    uint8_t port;
    uint8_t address;
    uint8_t ep0_size;
    uint8_t device_class;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t configuration_value;
    uint8_t interrupt_in_ep;
    uint8_t interrupt_in_packet;
    uint8_t bulk_in_ep;
    uint8_t bulk_out_ep;
    uint8_t bulk_packet;
    uint16_t vendor_id;
    uint16_t product_id;
    char inquiry[37];
};

struct usb_setup_packet {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} __attribute__((packed));

struct usb_msc_cbw {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_transfer_length;
    uint8_t flags;
    uint8_t lun;
    uint8_t cb_length;
    uint8_t cb[16];
} __attribute__((packed));

struct usb_msc_csw {
    uint32_t signature;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
} __attribute__((packed));

static struct usb_controller controllers[USB_MAX_CONTROLLERS];
static int controller_count;
static struct usb_device devices[USB_MAX_DEVICES];
static int device_count;
static uint8_t next_address;

static void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static uint16_t uhci_inw(struct usb_controller *ctrl, uint16_t reg) {
    return inw((uint16_t)(ctrl->io_base + reg));
}

static void uhci_outw(struct usb_controller *ctrl, uint16_t reg, uint16_t value) {
    outw((uint16_t)(ctrl->io_base + reg), value);
}

static void uhci_outl(struct usb_controller *ctrl, uint16_t reg, uint32_t value) {
    outl((uint16_t)(ctrl->io_base + reg), value);
}

static void usb_delay(void) {
    for (uint32_t i = 0; i < 10000; i++) {
        io_wait();
    }
}

static bool uhci_alloc_frame_list(struct usb_controller *ctrl) {
    void *phys = pmm_alloc_page();
    if (!phys) {
        return false;
    }
    ctrl->frame_list_phys = (uint64_t)phys;
    ctrl->frame_list = (uint32_t *)phys_to_virt((uint64_t)phys);
    for (uint32_t i = 0; i < UHCI_FRAME_COUNT; i++) {
        ctrl->frame_list[i] = 1U;
    }
    return true;
}

static uint32_t uhci_td_token(uint8_t pid, uint8_t address, uint8_t endpoint,
                              uint8_t toggle, uint16_t length) {
    uint32_t max_len = length == 0 ? 0x7FFU : (uint32_t)(length - 1);
    return ((max_len & 0x7FFU) << 21) |
           ((uint32_t)(toggle & 1U) << 19) |
           ((uint32_t)(endpoint & 0x0FU) << 15) |
           ((uint32_t)(address & 0x7FU) << 8) |
           pid;
}

static int uhci_run_transfer(struct usb_controller *ctrl,
                             bool low_speed,
                             struct uhci_td *tds,
                             uint64_t td_phys,
                             uint32_t td_count) {
    if (td_count == 0 || td_count > UHCI_MAX_TDS) {
        return -1;
    }

    for (uint32_t i = 0; i < td_count; i++) {
        uint32_t next = i + 1 < td_count
            ? (uint32_t)(td_phys + (i + 1) * sizeof(struct uhci_td))
            : UHCI_LINK_TERMINATE;
        tds[i].link = next;
        tds[i].status = UHCI_TD_STATUS_ACTIVE | UHCI_TD_STATUS_CERR |
                        (low_speed ? UHCI_TD_STATUS_LS : 0);
        if (i + 1 == td_count) {
            tds[i].status |= UHCI_TD_STATUS_IOC;
        }
    }

    uint64_t qh_phys = td_phys - sizeof(struct uhci_qh);
    struct uhci_qh *qh = (struct uhci_qh *)((uint8_t *)tds - sizeof(struct uhci_qh));
    qh->head = UHCI_LINK_TERMINATE;
    qh->element = (uint32_t)td_phys;

    ctrl->frame_list[0] = (uint32_t)qh_phys | UHCI_LINK_QH;

    for (uint32_t spin = 0; spin < 5000000; spin++) {
        bool done = true;
        for (uint32_t i = 0; i < td_count; i++) {
            if (tds[i].status & UHCI_TD_STATUS_ACTIVE) {
                done = false;
                break;
            }
        }
        if (!done) {
            continue;
        }

        ctrl->frame_list[0] = UHCI_LINK_TERMINATE;
        for (uint32_t i = 0; i < td_count; i++) {
            if (tds[i].status & UHCI_TD_ERROR_MASK) {
                return -1;
            }
        }
        return 0;
    }

    ctrl->frame_list[0] = UHCI_LINK_TERMINATE;
    return -1;
}

static int uhci_packet_transfer(struct usb_controller *ctrl,
                                bool low_speed,
                                uint8_t address,
                                uint8_t endpoint,
                                uint8_t pid,
                                uint8_t start_toggle,
                                uint64_t data_phys,
                                uint16_t length,
                                uint16_t max_packet) {
    void *page_virt = NULL;
    uint64_t page_phys = (uint64_t)pmm_alloc_page();
    if (!page_phys) {
        return -1;
    }
    page_virt = phys_to_virt(page_phys);
    memset(page_virt, 0, PAGE_SIZE);

    struct uhci_transfer_page *page = (struct uhci_transfer_page *)page_virt;
    uint16_t remaining = length;
    uint16_t offset = 0;
    uint8_t toggle = start_toggle;
    uint32_t td_count = 0;

    do {
        uint16_t chunk = remaining;
        if (chunk > max_packet) {
            chunk = max_packet;
        }
        if (td_count >= UHCI_MAX_TDS) {
            pmm_free_page((void *)page_phys);
            return -1;
        }
        page->td[td_count].token = uhci_td_token(pid, address, endpoint, toggle, chunk);
        page->td[td_count].buffer = (uint32_t)(data_phys + offset);
        td_count++;
        toggle ^= 1;
        offset += chunk;
        remaining -= chunk;
    } while (remaining > 0);

    int rc = uhci_run_transfer(ctrl, low_speed, page->td,
                               page_phys + sizeof(struct uhci_qh), td_count);
    pmm_free_page((void *)page_phys);
    return rc;
}

static int uhci_control_transfer(struct usb_controller *ctrl,
                                 bool low_speed,
                                 uint8_t address,
                                 uint8_t ep0_size,
                                 const struct usb_setup_packet *setup,
                                 void *data,
                                 uint16_t length) {
    void *setup_virt = NULL;
    uint64_t setup_phys = (uint64_t)pmm_alloc_page();
    if (!setup_phys) {
        return -1;
    }
    setup_virt = phys_to_virt(setup_phys);
    memset(setup_virt, 0, PAGE_SIZE);
    memcpy(setup_virt, setup, sizeof(*setup));

    void *data_virt = NULL;
    uint64_t data_phys = 0;
    if (length > 0) {
        data_phys = (uint64_t)pmm_alloc_page();
        if (!data_phys) {
            pmm_free_page((void *)setup_phys);
            return -1;
        }
        data_virt = phys_to_virt(data_phys);
        memset(data_virt, 0, PAGE_SIZE);
        if ((setup->request_type & USB_DIR_IN) == 0 && data) {
            memcpy(data_virt, data, length);
        }
    }

    void *page_virt = NULL;
    uint64_t page_phys = (uint64_t)pmm_alloc_page();
    if (!page_phys) {
        if (data_phys) pmm_free_page((void *)data_phys);
        pmm_free_page((void *)setup_phys);
        return -1;
    }
    page_virt = phys_to_virt(page_phys);
    memset(page_virt, 0, PAGE_SIZE);

    struct uhci_transfer_page *page = (struct uhci_transfer_page *)page_virt;
    uint32_t td_count = 0;
    page->td[td_count].token = uhci_td_token(USB_PID_SETUP, address, 0, 0, sizeof(*setup));
    page->td[td_count].buffer = (uint32_t)setup_phys;
    td_count++;

    uint8_t data_pid = (setup->request_type & USB_DIR_IN) ? USB_PID_IN : USB_PID_OUT;
    uint8_t status_pid = (setup->request_type & USB_DIR_IN) ? USB_PID_OUT : USB_PID_IN;
    uint16_t remaining = length;
    uint16_t offset = 0;
    uint8_t toggle = 1;
    while (remaining > 0) {
        uint16_t chunk = remaining > ep0_size ? ep0_size : remaining;
        if (td_count >= UHCI_MAX_TDS - 1) {
            pmm_free_page((void *)page_phys);
            if (data_phys) pmm_free_page((void *)data_phys);
            pmm_free_page((void *)setup_phys);
            return -1;
        }
        page->td[td_count].token = uhci_td_token(data_pid, address, 0, toggle, chunk);
        page->td[td_count].buffer = (uint32_t)(data_phys + offset);
        td_count++;
        toggle ^= 1;
        offset += chunk;
        remaining -= chunk;
    }

    page->td[td_count].token = uhci_td_token(status_pid, address, 0, 1, 0);
    page->td[td_count].buffer = 0;
    td_count++;

    int rc = uhci_run_transfer(ctrl, low_speed, page->td,
                               page_phys + sizeof(struct uhci_qh), td_count);
    if (rc == 0 && length > 0 && (setup->request_type & USB_DIR_IN) && data) {
        memcpy(data, data_virt, length);
    }

    pmm_free_page((void *)page_phys);
    if (data_phys) pmm_free_page((void *)data_phys);
    pmm_free_page((void *)setup_phys);
    return rc;
}

static bool uhci_reset_controller(struct usb_controller *ctrl) {
    uhci_outw(ctrl, UHCI_USBCMD, 0);
    for (uint32_t i = 0; i < 100000; i++) {
        if (uhci_inw(ctrl, UHCI_USBSTS) & UHCI_STS_HALTED) {
            break;
        }
    }

    uhci_outw(ctrl, UHCI_USBCMD, UHCI_CMD_HCRESET);
    for (uint32_t i = 0; i < 100000; i++) {
        if ((uhci_inw(ctrl, UHCI_USBCMD) & UHCI_CMD_HCRESET) == 0) {
            return true;
        }
    }
    return false;
}

static bool uhci_port_present(struct usb_controller *ctrl, uint8_t port) {
    uint16_t reg = port == 0 ? UHCI_PORTSC1 : UHCI_PORTSC2;
    uint16_t value = uhci_inw(ctrl, reg);
    return value != 0xFFFFU && value != 0x0000U;
}

static void uhci_reset_port(struct usb_controller *ctrl, uint8_t port) {
    uint16_t reg = port == 0 ? UHCI_PORTSC1 : UHCI_PORTSC2;
    uint16_t value = uhci_inw(ctrl, reg);
    if ((value & UHCI_PORT_CCS) == 0) {
        return;
    }

    uhci_outw(ctrl, reg, (uint16_t)((value & ~(UHCI_PORT_CSC | UHCI_PORT_PEC)) | UHCI_PORT_RESET));
    usb_delay();
    value = uhci_inw(ctrl, reg);
    uhci_outw(ctrl, reg, (uint16_t)(value & ~UHCI_PORT_RESET));
    usb_delay();
    value = uhci_inw(ctrl, reg);
    uhci_outw(ctrl, reg, (uint16_t)((value | UHCI_PORT_PE) & ~(UHCI_PORT_CSC | UHCI_PORT_PEC)));
    usb_delay();
}

static int usb_control_request(struct usb_device *dev,
                               uint8_t request_type,
                               uint8_t request,
                               uint16_t value,
                               uint16_t index,
                               void *data,
                               uint16_t length) {
    struct usb_setup_packet setup;
    setup.request_type = request_type;
    setup.request = request;
    setup.value = value;
    setup.index = index;
    setup.length = length;
    return uhci_control_transfer(&controllers[dev->controller],
                                 dev->low_speed,
                                 dev->address,
                                 dev->ep0_size,
                                 &setup,
                                 data,
                                 length);
}

static int usb_get_descriptor(struct usb_device *dev,
                              uint8_t type,
                              uint8_t index,
                              void *data,
                              uint16_t length) {
    return usb_control_request(dev, 0x80, USB_REQ_GET_DESCRIPTOR,
                               (uint16_t)((type << 8) | index), 0, data, length);
}

static int usb_set_address(struct usb_device *dev, uint8_t address) {
    struct usb_setup_packet setup;
    setup.request_type = 0x00;
    setup.request = USB_REQ_SET_ADDRESS;
    setup.value = address;
    setup.index = 0;
    setup.length = 0;
    int rc = uhci_control_transfer(&controllers[dev->controller],
                                   dev->low_speed, 0, dev->ep0_size,
                                   &setup, NULL, 0);
    if (rc == 0) {
        dev->address = address;
        usb_delay();
    }
    return rc;
}

static int usb_set_configuration(struct usb_device *dev, uint8_t configuration) {
    return usb_control_request(dev, 0x00, USB_REQ_SET_CONFIGURATION,
                               configuration, 0, NULL, 0);
}

static void usb_parse_configuration(struct usb_device *dev, const uint8_t *cfg, uint16_t length) {
    uint16_t offset = 0;
    dev->configuration_value = cfg[5];

    while (offset + 2 <= length) {
        uint8_t len = cfg[offset];
        uint8_t type = cfg[offset + 1];
        if (len < 2 || offset + len > length) {
            break;
        }

        if (type == USB_DT_INTERFACE && len >= 9 && dev->interface_class == 0) {
            dev->interface_class = cfg[offset + 5];
            dev->interface_subclass = cfg[offset + 6];
            dev->interface_protocol = cfg[offset + 7];
        } else if (type == USB_DT_ENDPOINT && len >= 7) {
            uint8_t endpoint = cfg[offset + 2];
            uint8_t attributes = cfg[offset + 3] & USB_ENDPOINT_TRANSFER_MASK;
            uint16_t max_packet = (uint16_t)(cfg[offset + 4] | ((uint16_t)cfg[offset + 5] << 8));
            if (attributes == USB_ENDPOINT_INTERRUPT && (endpoint & USB_DIR_IN)) {
                dev->interrupt_in_ep = endpoint & USB_ENDPOINT_NUMBER_MASK;
                dev->interrupt_in_packet = (uint8_t)(max_packet > 255 ? 255 : max_packet);
            } else if (attributes == USB_ENDPOINT_BULK && (endpoint & USB_DIR_IN)) {
                dev->bulk_in_ep = endpoint & USB_ENDPOINT_NUMBER_MASK;
                dev->bulk_packet = (uint8_t)(max_packet > 255 ? 255 : max_packet);
            } else if (attributes == USB_ENDPOINT_BULK) {
                dev->bulk_out_ep = endpoint & USB_ENDPOINT_NUMBER_MASK;
                dev->bulk_packet = (uint8_t)(max_packet > 255 ? 255 : max_packet);
            }
        }

        offset += len;
    }
}

static void usb_init_hid(struct usb_device *dev) {
    if (dev->interface_class != USB_CLASS_HID) {
        return;
    }

    if (dev->interface_subclass == 1) {
        usb_control_request(dev, 0x21, USB_REQ_SET_PROTOCOL, 0, 0, NULL, 0);
    }
    usb_control_request(dev, 0x21, USB_REQ_SET_IDLE, 0, 0, NULL, 0);
    dev->hid_ready = dev->interrupt_in_ep != 0;
}

static int usb_bulk_transfer(struct usb_device *dev,
                             uint8_t endpoint,
                             uint8_t pid,
                             uint8_t toggle,
                             void *data,
                             uint16_t length) {
    void *bounce = NULL;
    uint64_t bounce_phys = (uint64_t)pmm_alloc_page();
    if (!bounce_phys) {
        return -1;
    }
    bounce = phys_to_virt(bounce_phys);
    memset(bounce, 0, PAGE_SIZE);
    if (pid == USB_PID_OUT && data && length > 0) {
        memcpy(bounce, data, length);
    }

    int rc = uhci_packet_transfer(&controllers[dev->controller],
                                  dev->low_speed,
                                  dev->address,
                                  endpoint,
                                  pid,
                                  toggle,
                                  bounce_phys,
                                  length,
                                  dev->bulk_packet ? dev->bulk_packet : 64);
    if (rc == 0 && pid == USB_PID_IN && data && length > 0) {
        memcpy(data, bounce, length);
    }
    pmm_free_page((void *)bounce_phys);
    return rc;
}

static void usb_init_mass_storage(struct usb_device *dev) {
    if (dev->interface_class != USB_CLASS_MASS_STORAGE ||
        dev->interface_subclass != 0x06 ||
        dev->interface_protocol != 0x50 ||
        dev->bulk_in_ep == 0 ||
        dev->bulk_out_ep == 0) {
        return;
    }

    struct usb_msc_cbw cbw;
    struct usb_msc_csw csw;
    uint8_t inquiry[36];
    memset(&cbw, 0, sizeof(cbw));
    memset(&csw, 0, sizeof(csw));
    memset(inquiry, 0, sizeof(inquiry));

    cbw.signature = USB_MSC_CBW_SIGNATURE;
    cbw.tag = USB_MSC_CBW_TAG;
    cbw.data_transfer_length = sizeof(inquiry);
    cbw.flags = USB_DIR_IN;
    cbw.cb_length = 6;
    cbw.cb[0] = 0x12;
    cbw.cb[4] = sizeof(inquiry);

    if (usb_bulk_transfer(dev, dev->bulk_out_ep, USB_PID_OUT, 0, &cbw, sizeof(cbw)) == 0 &&
        usb_bulk_transfer(dev, dev->bulk_in_ep, USB_PID_IN, 1, inquiry, sizeof(inquiry)) == 0 &&
        usb_bulk_transfer(dev, dev->bulk_in_ep, USB_PID_IN, 0, &csw, sizeof(csw)) == 0 &&
        csw.signature == USB_MSC_CSW_SIGNATURE &&
        csw.status == 0) {
        for (uint32_t i = 0; i < 36; i++) {
            char c = (char)inquiry[i];
            dev->inquiry[i] = (c >= 32 && c < 127) ? c : ' ';
        }
        dev->inquiry[36] = '\0';
        dev->mass_ready = true;
    }
}

static void usb_enumerate_port(struct usb_controller *ctrl, uint8_t controller_index, uint8_t port) {
    if (device_count >= USB_MAX_DEVICES || !usb_port_connected(controller_index, port)) {
        return;
    }

    uint16_t portsc = uhci_inw(ctrl, port == 0 ? UHCI_PORTSC1 : UHCI_PORTSC2);
    struct usb_device *dev = &devices[device_count];
    memset(dev, 0, sizeof(*dev));
    dev->present = true;
    dev->controller = controller_index;
    dev->port = port;
    dev->low_speed = (portsc & UHCI_PORT_LSDA) != 0;
    dev->address = 0;
    dev->ep0_size = 8;

    uint8_t desc[18];
    memset(desc, 0, sizeof(desc));
    if (usb_get_descriptor(dev, USB_DT_DEVICE, 0, desc, 8) < 0) {
        dev->present = false;
        return;
    }

    dev->ep0_size = desc[7] ? desc[7] : 8;
    uint8_t address = next_address++;
    if (usb_set_address(dev, address) < 0 ||
        usb_get_descriptor(dev, USB_DT_DEVICE, 0, desc, sizeof(desc)) < 0) {
        dev->present = false;
        return;
    }

    dev->device_class = desc[4];
    dev->vendor_id = (uint16_t)(desc[8] | ((uint16_t)desc[9] << 8));
    dev->product_id = (uint16_t)(desc[10] | ((uint16_t)desc[11] << 8));

    uint8_t cfg[USB_MAX_CONTROL_BYTES];
    memset(cfg, 0, sizeof(cfg));
    if (usb_get_descriptor(dev, USB_DT_CONFIGURATION, 0, cfg, 9) < 0) {
        dev->present = false;
        return;
    }
    uint16_t total = (uint16_t)(cfg[2] | ((uint16_t)cfg[3] << 8));
    if (total > sizeof(cfg)) {
        total = sizeof(cfg);
    }
    if (usb_get_descriptor(dev, USB_DT_CONFIGURATION, 0, cfg, total) < 0) {
        dev->present = false;
        return;
    }

    usb_parse_configuration(dev, cfg, total);
    if (usb_set_configuration(dev, dev->configuration_value) < 0) {
        dev->present = false;
        return;
    }

    usb_init_hid(dev);
    usb_init_mass_storage(dev);

    serial_puts("USB: device addr=");
    pci_log_dec(dev->address);
    serial_puts(" vid=0x");
    pci_log_hex16(dev->vendor_id);
    serial_puts(" pid=0x");
    pci_log_hex16(dev->product_id);
    serial_puts(" class=0x");
    pci_log_hex8(dev->interface_class);
    serial_puts(dev->hid_ready ? " HID-ready" : "");
    serial_puts(dev->mass_ready ? " MSC-ready" : "");
    serial_puts("\n");

    device_count++;
}

static bool uhci_start(struct usb_controller *ctrl) {
    if (!uhci_alloc_frame_list(ctrl) || !uhci_reset_controller(ctrl)) {
        return false;
    }

    uhci_outw(ctrl, UHCI_USBINTR, 0);
    uhci_outw(ctrl, UHCI_USBSTS, 0xFFFF);
    uhci_outw(ctrl, UHCI_FRNUM, 0);
    uhci_outl(ctrl, UHCI_FLBASEADD, (uint32_t)ctrl->frame_list_phys);
    outb((uint16_t)(ctrl->io_base + UHCI_SOFMOD), 0x40);
    uhci_outw(ctrl, UHCI_USBCMD, UHCI_CMD_CF | UHCI_CMD_RS);

    ctrl->port_count = 0;
    for (uint8_t port = 0; port < 2; port++) {
        if (uhci_port_present(ctrl, port)) {
            ctrl->port_count++;
            uhci_reset_port(ctrl, port);
        }
    }
    return true;
}

void usb_init(void) {
    controller_count = 0;
    device_count = 0;
    next_address = 1;
    memset(controllers, 0, sizeof(controllers));
    memset(devices, 0, sizeof(devices));
    serial_puts("USB: registered UHCI root-port driver; OHCI/EHCI/XHCI remain diagnostics-only\n");
}

void uhci_init(void) {
    serial_puts("UHCI: registered controller init, frame list, run/stop, and root-port reset\n");
}

void ohci_init(void) {
    serial_puts("OHCI: registered PCI diagnostics; HCCA and endpoint lists unsupported\n");
}

void ehci_init(void) {
    serial_puts("EHCI: registered PCI diagnostics; async/periodic schedules unsupported\n");
}

void xhci_init(void) {
    serial_puts("XHCI: registered PCI diagnostics; command/event rings unsupported\n");
}

int uhci_probe(const struct pci_device *dev) {
    if (controller_count >= USB_MAX_CONTROLLERS) {
        return -1;
    }

    struct pci_bar bar;
    if (pci_read_bar(dev, 4, &bar) != 0 || !bar.present || !bar.is_io) {
        serial_puts("UHCI: BAR4 I/O base missing\n");
        return -1;
    }

    uint16_t command = pci_config_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_IO | PCI_COMMAND_BUS_MASTER;
    pci_config_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, command);

    struct usb_controller *ctrl = &controllers[controller_count];
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->present = true;
    ctrl->type = USB_CTRL_UHCI;
    ctrl->io_base = (uint16_t)bar.address;

    serial_puts("UHCI: probing controller ");
    pci_log_location(dev);
    serial_puts(" io=0x");
    pci_log_hex16(ctrl->io_base);
    serial_puts("\n");

    if (!uhci_start(ctrl)) {
        serial_puts("UHCI: initialization failed\n");
        ctrl->present = false;
        return -1;
    }

    uint8_t controller_index = (uint8_t)controller_count;
    controller_count++;
    serial_puts("UHCI: running ports=");
    pci_log_dec(ctrl->port_count);
    serial_puts("\n");
    for (uint8_t port = 0; port < 2; port++) {
        if (uhci_port_present(ctrl, port)) {
            usb_enumerate_port(ctrl, controller_index, port);
        }
    }
    return 0;
}

static int usb_probe_diagnostics(const struct pci_device *dev, const char *name) {
    serial_puts("USB: ");
    serial_puts(name);
    serial_puts(" controller ");
    pci_log_location(dev);
    serial_puts(" detected; operational driver not implemented\n");
    pci_log_bars(dev, "USB: ");
    pci_log_capabilities(dev, "USB: ");
    return 0;
}

int ohci_probe(const struct pci_device *dev) {
    return usb_probe_diagnostics(dev, "OHCI");
}

int ehci_probe(const struct pci_device *dev) {
    return usb_probe_diagnostics(dev, "EHCI");
}

int xhci_probe(const struct pci_device *dev) {
    return usb_probe_diagnostics(dev, "XHCI");
}

int usb_controller_count(void) {
    return controller_count;
}

int usb_port_count(int controller) {
    if (controller < 0 || controller >= controller_count || !controllers[controller].present) {
        return 0;
    }
    return controllers[controller].port_count;
}

bool usb_port_connected(int controller, int port) {
    if (controller < 0 || controller >= controller_count || port < 0 || port >= usb_port_count(controller)) {
        return false;
    }
    struct usb_controller *ctrl = &controllers[controller];
    if (ctrl->type != USB_CTRL_UHCI) {
        return false;
    }
    uint16_t reg = port == 0 ? UHCI_PORTSC1 : UHCI_PORTSC2;
    return (uhci_inw(ctrl, reg) & UHCI_PORT_CCS) != 0;
}

bool usb_port_enabled(int controller, int port) {
    if (controller < 0 || controller >= controller_count || port < 0 || port >= usb_port_count(controller)) {
        return false;
    }
    struct usb_controller *ctrl = &controllers[controller];
    if (ctrl->type != USB_CTRL_UHCI) {
        return false;
    }
    uint16_t reg = port == 0 ? UHCI_PORTSC1 : UHCI_PORTSC2;
    return (uhci_inw(ctrl, reg) & UHCI_PORT_PE) != 0;
}

int usb_device_count(void) {
    return device_count;
}

const char *usb_device_class_name(int index) {
    if (index < 0 || index >= device_count || !devices[index].present) {
        return "none";
    }
    if (devices[index].hid_ready) {
        return "HID";
    }
    if (devices[index].mass_ready) {
        return "mass-storage";
    }
    switch (devices[index].interface_class) {
        case USB_CLASS_HID:
            return "HID";
        case USB_CLASS_MASS_STORAGE:
            return "mass-storage";
        default:
            return "generic";
    }
}

bool usb_device_is_hid(int index) {
    return index >= 0 && index < device_count && devices[index].hid_ready;
}

bool usb_device_is_mass_storage(int index) {
    return index >= 0 && index < device_count && devices[index].mass_ready;
}

uint16_t usb_device_vendor(int index) {
    if (index < 0 || index >= device_count) {
        return 0;
    }
    return devices[index].vendor_id;
}

uint16_t usb_device_product(int index) {
    if (index < 0 || index >= device_count) {
        return 0;
    }
    return devices[index].product_id;
}
