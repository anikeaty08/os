/*
 * AstraOS - Kernel Entry Point
 * Main kernel initialization and entry
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "limine.h"
#include "panic.h"
#include "lib/string.h"
#include "lib/stdio.h"
#include "drivers/serial.h"
#include "drivers/pit.h"
#include "drivers/keyboard.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "drivers/ata.h"
#include "drivers/acpi.h"
#include "fs/vfs.h"
#include "fs/fat.h"
#include "shell/shell.h"
#include "shell/user.h"
#include "lib/theme.h"

/*
 * Limine Request Markers
 * These MUST be in the .limine_requests section
 */
__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

/*
 * Base Revision
 * Must be present for Limine to recognize the kernel
 */
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

/*
 * Framebuffer Request
 */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

/*
 * HHDM Request (Higher Half Direct Map)
 */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

/*
 * Memory Map Request
 */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

/*
 * Kernel Address Request
 */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kernel_addr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

/*
 * Bootloader Info Request
 */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0
};

/*
 * RSDP (ACPI) Request
 */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

/*
 * Global HHDM offset for physical to virtual address conversion
 */
uint64_t hhdm_offset = 0;

/*
 * Global framebuffer pointer
 */
static struct limine_framebuffer *g_framebuffer = NULL;

/*
 * Simple hex number to string conversion
 */
static void uint64_to_hex(uint64_t value, char *buf) {
    const char *hex = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buf[2 + (15 - i)] = hex[(value >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
}

/*
 * Simple decimal number to string
 */
static void uint64_to_dec(uint64_t value, char *buf) {
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    char temp[21];
    int i = 0;
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

/*
 * Draw a pixel to the framebuffer
 */
static void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_framebuffer) return;
    if (x >= g_framebuffer->width || y >= g_framebuffer->height) return;

    volatile uint32_t *pixel = (volatile uint32_t *)((uint8_t *)g_framebuffer->address +
                                                      y * g_framebuffer->pitch +
                                                      x * (g_framebuffer->bpp / 8));
    *pixel = color;
}

/*
 * Draw a simple character (8x8 font embedded)
 * Very basic font for initial testing
 */
static const uint8_t font_8x8[128][8] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
    ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4'] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
    ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6'] = {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
    ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00},
    ['A'] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
    ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D'] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['K'] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N'] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00},
    ['S'] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['a'] = {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
    ['b'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00},
    ['d'] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
    ['f'] = {0x1C, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x00},
    ['g'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C},
    ['h'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['k'] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m'] = {0x00, 0x00, 0x76, 0x7F, 0x6B, 0x6B, 0x63, 0x00},
    ['n'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
    ['r'] = {0x00, 0x00, 0x6C, 0x76, 0x60, 0x60, 0x60, 0x00},
    ['s'] = {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
    ['t'] = {0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00},
    ['u'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['v'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['w'] = {0x00, 0x00, 0x63, 0x6B, 0x6B, 0x7F, 0x36, 0x00},
    ['x'] = {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
    ['y'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C},
    ['z'] = {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    [':'] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    [','] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    ['('] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
    [')'] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    ['/'] = {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00},
    ['\n'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static uint32_t fb_cursor_x = 0;
static uint32_t fb_cursor_y = 0;
static const uint32_t CHAR_WIDTH = 8;
static const uint32_t CHAR_HEIGHT = 8;
static const uint32_t FG_COLOR = 0x00FF00;  /* Green text */
static const uint32_t BG_COLOR = 0x000000;  /* Black background */

static void fb_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    if (!g_framebuffer) return;

    const uint8_t *glyph = font_8x8[(uint8_t)c];

    for (uint32_t py = 0; py < CHAR_HEIGHT; py++) {
        for (uint32_t px = 0; px < CHAR_WIDTH; px++) {
            uint32_t color = (glyph[py] & (0x80 >> px)) ? fg : bg;
            fb_putpixel(x + px, y + py, color);
        }
    }
}

static void fb_scroll(void) {
    if (!g_framebuffer) return;

    uint8_t *fb = (uint8_t *)g_framebuffer->address;
    uint64_t row_size = g_framebuffer->pitch * CHAR_HEIGHT;
    uint64_t total_size = g_framebuffer->pitch * g_framebuffer->height;

    /* Move all rows up by one character height */
    memmove(fb, fb + row_size, total_size - row_size);

    /* Clear the last row */
    memset(fb + total_size - row_size, 0, row_size);

    fb_cursor_y -= CHAR_HEIGHT;
}

void fb_putchar(char c) {
    if (!g_framebuffer) return;

    if (c == '\n') {
        fb_cursor_x = 0;
        fb_cursor_y += CHAR_HEIGHT;
    } else if (c == '\r') {
        fb_cursor_x = 0;
    } else if (c == '\t') {
        fb_cursor_x = (fb_cursor_x + 32) & ~31;
    } else if (c == '\b') {
        if (fb_cursor_x >= CHAR_WIDTH) {
            fb_cursor_x -= CHAR_WIDTH;
        } else if (fb_cursor_y >= CHAR_HEIGHT) {
            fb_cursor_y -= CHAR_HEIGHT;
            fb_cursor_x = (g_framebuffer->width / CHAR_WIDTH - 1) * CHAR_WIDTH;
        }
    } else {
        fb_draw_char(c, fb_cursor_x, fb_cursor_y, FG_COLOR, BG_COLOR);
        fb_cursor_x += CHAR_WIDTH;
    }

    /* Wrap at end of line */
    if (fb_cursor_x + CHAR_WIDTH > g_framebuffer->width) {
        fb_cursor_x = 0;
        fb_cursor_y += CHAR_HEIGHT;
    }

    /* Scroll if at bottom */
    if (fb_cursor_y + CHAR_HEIGHT > g_framebuffer->height) {
        fb_scroll();
    }
}

void fb_puts(const char *s) {
    while (*s) {
        fb_putchar(*s++);
    }
}

/*
 * Get framebuffer dimensions (in characters)
 */
uint32_t fb_get_width(void) {
    if (!g_framebuffer) return 80;
    return g_framebuffer->width / CHAR_WIDTH;
}

uint32_t fb_get_height(void) {
    if (!g_framebuffer) return 25;
    return g_framebuffer->height / CHAR_HEIGHT;
}

/*
 * Calculate X position to center text
 */
uint32_t fb_center_x(const char *text) {
    uint32_t screen_w = fb_get_width();
    uint32_t screen_w = fb_get_width();
    
    /* Strip ANSI codes for length calculation (simplified) */
    uint32_t visible_len = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '\033') {
            while (text[i] && text[i] != 'm') i++;
        } else {
            visible_len++;
        }
    }
    
    if (visible_len >= screen_w) return 0;
    return (screen_w - visible_len) / 2;
}

/*
 * Calculate Y position to center content of given height
 */
uint32_t fb_center_y(uint32_t content_height) {
    uint32_t screen_h = fb_get_height();
    if (content_height >= screen_h) return 0;
    return (screen_h - content_height) / 2;
}

void fb_clear(void) {
    if (!g_framebuffer) return;

    memset(g_framebuffer->address, 0,
           g_framebuffer->pitch * g_framebuffer->height);
    fb_cursor_x = 0;
    fb_cursor_y = 0;
}

/*
 * Print memory size in human-readable format
 */
static void print_memory_size(uint64_t bytes) {
    char buf[32];

    if (bytes >= 1024ULL * 1024 * 1024) {
        uint64_to_dec(bytes / (1024 * 1024 * 1024), buf);
        serial_puts(buf);
        serial_puts(" GB");
    } else if (bytes >= 1024 * 1024) {
        uint64_to_dec(bytes / (1024 * 1024), buf);
        serial_puts(buf);
        serial_puts(" MB");
    } else if (bytes >= 1024) {
        uint64_to_dec(bytes / 1024, buf);
        serial_puts(buf);
        serial_puts(" KB");
    } else {
        uint64_to_dec(bytes, buf);
        serial_puts(buf);
        serial_puts(" bytes");
    }
}

/*
 * Memory type to string
 */
static const char *memmap_type_str(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE:                 return "Usable";
        case LIMINE_MEMMAP_RESERVED:               return "Reserved";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return "ACPI Reclaimable";
        case LIMINE_MEMMAP_ACPI_NVS:               return "ACPI NVS";
        case LIMINE_MEMMAP_BAD_MEMORY:             return "Bad Memory";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "Bootloader Reclaimable";
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:     return "Kernel/Modules";
        case LIMINE_MEMMAP_FRAMEBUFFER:            return "Framebuffer";
        default:                                    return "Unknown";
    }
}

/*
 * Kernel Entry Point
 * Called by Limine bootloader after setting up long mode
 */
void kmain(void) {
    char buf[32];

    /* Initialize serial port first for early debug output */
    serial_init();

    serial_puts("\n");
    serial_puts("========================================\n");
    serial_puts("  AstraOS Kernel Starting...\n");
    serial_puts("========================================\n");
    serial_puts("\n");

    /* Get bootloader info */
    if (bootloader_info_request.response) {
        serial_puts("Bootloader: ");
        serial_puts(bootloader_info_request.response->name);
        serial_puts(" ");
        serial_puts(bootloader_info_request.response->version);
        serial_puts("\n");
    }

    /* Get HHDM offset */
    if (!hhdm_request.response) {
        panic("Failed to get HHDM response from bootloader");
    }
    hhdm_offset = hhdm_request.response->offset;
    serial_puts("HHDM Offset: ");
    uint64_to_hex(hhdm_offset, buf);
    serial_puts(buf);
    serial_puts("\n");

    /* Initialize framebuffer */
    if (!framebuffer_request.response ||
        framebuffer_request.response->framebuffer_count < 1) {
        panic("Failed to get framebuffer from bootloader");
    }

    g_framebuffer = framebuffer_request.response->framebuffers[0];

    serial_puts("Framebuffer: ");
    uint64_to_dec(g_framebuffer->width, buf);
    serial_puts(buf);
    serial_puts("x");
    uint64_to_dec(g_framebuffer->height, buf);
    serial_puts(buf);
    serial_puts(" @ ");
    uint64_to_dec(g_framebuffer->bpp, buf);
    serial_puts(buf);
    serial_puts(" bpp\n");

    /* Clear screen and show welcome message */
    fb_clear();
    fb_puts("AstraOS v0.1\n");
    fb_puts("============\n\n");

    /* Initialize GDT */
    serial_puts("\nInitializing GDT... ");
    gdt_init();
    serial_puts("OK\n");
    fb_puts("GDT initialized\n");

    /* Initialize IRQ subsystem (PIC) */
    serial_puts("Initializing IRQ (PIC)... ");
    irq_init();
    serial_puts("OK\n");
    fb_puts("PIC initialized\n");

    /* Initialize IDT */
    serial_puts("Initializing IDT... ");
    idt_init();
    serial_puts("OK\n");
    fb_puts("IDT initialized\n");

    /* Enable interrupts */
    serial_puts("Enabling interrupts... ");
    cpu_sti();
    serial_puts("OK\n");
    fb_puts("Interrupts enabled\n\n");

    /* Get kernel address info */
    if (kernel_addr_request.response) {
        serial_puts("Kernel Physical: ");
        uint64_to_hex(kernel_addr_request.response->physical_base, buf);
        serial_puts(buf);
        serial_puts("\n");

        serial_puts("Kernel Virtual:  ");
        uint64_to_hex(kernel_addr_request.response->virtual_base, buf);
        serial_puts(buf);
        serial_puts("\n");
    }

    /* Parse and display memory map */
    if (!memmap_request.response) {
        panic("Failed to get memory map from bootloader");
    }

    serial_puts("\nMemory Map:\n");
    serial_puts("-----------\n");

    uint64_t total_usable = 0;
    uint64_t total_memory = 0;

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];

        serial_puts("  ");
        uint64_to_hex(entry->base, buf);
        serial_puts(buf);
        serial_puts(" - ");
        uint64_to_hex(entry->base + entry->length, buf);
        serial_puts(buf);
        serial_puts(" : ");
        serial_puts(memmap_type_str(entry->type));
        serial_puts(" (");
        print_memory_size(entry->length);
        serial_puts(")\n");

        total_memory += entry->length;
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_usable += entry->length;
        }
    }

    serial_puts("\nTotal Memory: ");
    print_memory_size(total_memory);
    serial_puts("\n");

    serial_puts("Usable Memory: ");
    print_memory_size(total_usable);
    serial_puts("\n");

    /* Initialize Physical Memory Manager */
    serial_puts("\nInitializing PMM... ");
    pmm_init(memmap_request.response, hhdm_offset);
    serial_puts("OK\n");
    fb_puts("Physical memory manager initialized\n");

    /* Initialize Virtual Memory Manager */
    serial_puts("Initializing VMM... ");
    vmm_init(hhdm_offset);
    serial_puts("OK\n");
    fb_puts("Virtual memory manager initialized\n");

    /* Initialize Kernel Heap */
    serial_puts("Initializing heap... ");
    heap_init();
    serial_puts("OK\n");
    fb_puts("Kernel heap initialized\n");

    /* Initialize PIT Timer */
    serial_puts("Initializing PIT timer... ");
    pit_init(1000);  /* 1000 Hz = 1ms per tick */
    serial_puts("OK\n");
    fb_puts("PIT timer initialized (1000 Hz)\n");

    /* Initialize Keyboard */
    serial_puts("Initializing keyboard... ");
    keyboard_init();
    serial_puts("OK\n");
    fb_puts("PS/2 keyboard initialized\n");

    /* Initialize Process Management */
    serial_puts("Initializing process management... ");
    process_init();
    scheduler_init();
    serial_puts("OK\n");
    fb_puts("Process management initialized\n");

    /* Initialize ACPI */
    serial_puts("Initializing ACPI...\n");
    void *rsdp_addr = NULL;
    if (rsdp_request.response && rsdp_request.response->address) {
        void *raw_addr = rsdp_request.response->address;
        /* Limine may return a physical address for RSDP - convert via HHDM if needed */
        if ((uint64_t)raw_addr < hhdm_offset) {
            rsdp_addr = (void *)((uint64_t)raw_addr + hhdm_offset);
        } else {
            rsdp_addr = raw_addr;
        }
        serial_puts("ACPI: RSDP provided by bootloader\n");
    } else {
        serial_puts("ACPI: No RSDP from bootloader, will scan BIOS ROM\n");
    }
    if (acpi_init(rsdp_addr, hhdm_offset)) {
        serial_puts("ACPI: Initialized successfully\n");
        fb_puts("ACPI initialized (power off supported)\n");
    } else {
        serial_puts("ACPI: Not available\n");
        fb_puts("ACPI not available (using fallback shutdown)\n");
    }

    /* Initialize ATA/Disk */
    serial_puts("Initializing ATA... ");
    ata_init();
    serial_puts("OK\n");
    fb_puts("ATA disk driver initialized\n");

    /* Initialize VFS */
    serial_puts("Initializing VFS... ");
    vfs_init();
    serial_puts("OK\n");

    /* Try to mount FAT16 filesystem */
    serial_puts("Detecting FAT16 filesystem... ");
    struct vfs_node *fs_root = NULL;

    /* Try each ATA drive */
    for (int i = 0; i < 4; i++) {
        if (ata_drive_present(i)) {
            fs_root = fat16_init(i, 0);  /* Try partition at LBA 0 */
            if (fs_root) {
                vfs_mount_root(fs_root);
                serial_puts("OK (drive ");
                char drive_num = '0' + i;
                serial_putchar(drive_num);
                serial_puts(")\n");
                fb_puts("FAT16 filesystem mounted\n");
                break;
            }
        }
    }

    if (!fs_root) {
        serial_puts("No FAT16 filesystem found\n");
        fb_puts("No filesystem detected (ls/cat disabled)\n");
    }

    /* Display memory info */
    fb_puts("\nMemory: ");
    uint64_to_dec(pmm_get_free_memory() / (1024 * 1024), buf);
    fb_puts(buf);
    fb_puts(" MB free / ");
    uint64_to_dec(pmm_get_total_memory() / (1024 * 1024), buf);
    fb_puts(buf);
    fb_puts(" MB total\n");

    serial_puts("\n========================================\n");
    serial_puts("  AstraOS Kernel Initialized!\n");
    serial_puts("  All subsystems operational.\n");
    serial_puts("========================================\n");

    fb_puts("\nAll systems initialized successfully!\n");
    fb_puts("Starting shell...\n");

    /* Initialize subsystems */
    theme_init();
    user_system_init();
    
    /* Play boot animation */
    /* Start the interactive shell */
    shell_run();

    /* Should never reach here */
    panic("Shell exited unexpectedly");
}
