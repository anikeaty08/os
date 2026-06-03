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
#define UHCI_FRAME_COUNT            1024

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

static struct usb_controller controllers[USB_MAX_CONTROLLERS];
static int controller_count;

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
    memset(controllers, 0, sizeof(controllers));
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

    serial_puts("UHCI: running ports=");
    pci_log_dec(ctrl->port_count);
    serial_puts("\n");
    controller_count++;
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
