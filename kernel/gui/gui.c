/*
 * AstraOS - Framebuffer GUI
 * Keyboard-driven desktop panels backed by live kernel state.
 */

#include "gui.h"
#include "../arch/x86_64/apic.h"
#include "../drivers/acpi.h"
#include "../drivers/graphics.h"
#include "../drivers/pci.h"
#include "../drivers/pit.h"
#include "../fs/vfs.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../mm/heap.h"
#include "../mm/pmm.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../shell/user.h"

#define GUI_BG          0x101418
#define GUI_TOP         0x1D252C
#define GUI_SIDE        0x151B20
#define GUI_PANEL       0x202930
#define GUI_PANEL_ALT   0x26323A
#define GUI_BORDER      0x3E4A52
#define GUI_ACCENT      0x2EA8A1
#define GUI_WARN        0xD7A23A
#define GUI_BAD         0xD86A6A
#define GUI_TEXT        0xE8EEF2
#define GUI_MUTED       0x9CAAB3
#define GUI_OK          0x7CCB83

#define TOP_H           28
#define FOOT_H          22
#define SIDE_W          136
#define PAD             12
#define LINE_H          14

enum gui_view {
    GUI_VIEW_FILES = 0,
    GUI_VIEW_SYSTEM,
    GUI_VIEW_DRIVERS,
    GUI_VIEW_PROCESSES,
    GUI_VIEW_USERS,
    GUI_VIEW_COUNT
};

static const char *view_names[GUI_VIEW_COUNT] = {
    "Files",
    "System",
    "Drivers",
    "Processes",
    "Users",
};

static uint32_t screen_w(void) {
    return fb_get_pixel_width();
}

static uint32_t screen_h(void) {
    return fb_get_pixel_height();
}

static void text_px(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg) {
    fb_draw_text_px(x, y, s, fg, bg);
}

static void clipped_text(uint32_t x, uint32_t y, uint32_t max_chars,
                         const char *s, uint32_t fg, uint32_t bg) {
    char buf[96];
    if (!s) {
        return;
    }

    if (max_chars >= sizeof(buf)) {
        max_chars = sizeof(buf) - 1;
    }
    strncpy(buf, s, max_chars);
    buf[max_chars] = '\0';
    text_px(x, y, buf, fg, bg);
}

static void label_value(uint32_t x, uint32_t y, const char *label, const char *value) {
    char buf[128];
    ksnprintf(buf, sizeof(buf), "%s: %s", label, value);
    text_px(x, y, buf, GUI_TEXT, GUI_PANEL);
}

static void label_u64(uint32_t x, uint32_t y, const char *label, uint64_t value) {
    char buf[128];
    ksnprintf(buf, sizeof(buf), "%s: %llu", label, value);
    text_px(x, y, buf, GUI_TEXT, GUI_PANEL);
}

static void label_mb(uint32_t x, uint32_t y, const char *label, uint64_t bytes) {
    char buf[128];
    ksnprintf(buf, sizeof(buf), "%s: %llu MB", label, bytes / (1024 * 1024));
    text_px(x, y, buf, GUI_TEXT, GUI_PANEL);
}

static void draw_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char *title) {
    if (w < 4 || h < 22) {
        return;
    }

    fb_fill_rect(x, y, w, h, GUI_PANEL);
    fb_draw_rect(x, y, w, h, GUI_BORDER);
    fb_fill_rect(x + 1, y + 1, w - 2, 18, GUI_PANEL_ALT);
    text_px(x + 8, y + 5, title, GUI_TEXT, GUI_PANEL_ALT);
}

static void draw_topbar(enum gui_view active) {
    char buf[160];
    uint64_t seconds = pit_get_ticks() / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;

    fb_fill_rect(0, 0, screen_w(), TOP_H, GUI_TOP);
    fb_fill_rect(0, TOP_H - 2, screen_w(), 2, GUI_ACCENT);
    text_px(10, 9, "AstraOS Desktop", GUI_TEXT, GUI_TOP);
    ksnprintf(buf, sizeof(buf), "%s | %s | up %02llu:%02llu:%02llu",
              view_names[active], user_get_current_name(), hours, minutes, seconds);
    uint32_t x = screen_w() > 360 ? screen_w() - 340 : 170;
    text_px(x, 9, buf, GUI_MUTED, GUI_TOP);
}

static void draw_sidebar(enum gui_view active) {
    uint32_t h = screen_h();
    if (h <= TOP_H + FOOT_H) {
        return;
    }

    fb_fill_rect(0, TOP_H, SIDE_W, h - TOP_H - FOOT_H, GUI_SIDE);
    for (uint32_t i = 0; i < GUI_VIEW_COUNT; i++) {
        uint32_t y = TOP_H + PAD + (i * 34);
        uint32_t bg = (i == (uint32_t)active) ? GUI_ACCENT : GUI_SIDE;
        uint32_t fg = (i == (uint32_t)active) ? 0x071011 : GUI_TEXT;
        fb_fill_rect(8, y, SIDE_W - 16, 24, bg);
        fb_draw_rect(8, y, SIDE_W - 16, 24, (i == (uint32_t)active) ? GUI_ACCENT : GUI_BORDER);
        char item[32];
        ksnprintf(item, sizeof(item), "%u %s", i + 1, view_names[i]);
        text_px(18, y + 8, item, fg, bg);
    }
}

static void draw_footer(void) {
    if (screen_h() < FOOT_H) {
        return;
    }

    uint32_t y = screen_h() - FOOT_H;
    fb_fill_rect(0, y, screen_w(), FOOT_H, GUI_TOP);
    text_px(10, y + 7, "1 Files  2 System  3 Drivers  4 Processes  5 Users  R Refresh  Q Console",
            GUI_MUTED, GUI_TOP);
}

static uint32_t content_x(void) {
    return SIDE_W + PAD;
}

static uint32_t content_y(void) {
    return TOP_H + PAD;
}

static uint32_t content_w(void) {
    uint32_t w = screen_w();
    return w > SIDE_W + (PAD * 2) ? w - SIDE_W - (PAD * 2) : 1;
}

static uint32_t content_h(void) {
    uint32_t h = screen_h();
    return h > TOP_H + FOOT_H + (PAD * 2) ? h - TOP_H - FOOT_H - (PAD * 2) : 1;
}

static void draw_files(void) {
    uint32_t x = content_x();
    uint32_t y = content_y();
    uint32_t w = content_w();
    uint32_t h = content_h();
    draw_panel(x, y, w, h, "Root Filesystem");

    struct vfs_node *root = vfs_get_root();
    if (!root) {
        text_px(x + 10, y + 34, "No root filesystem mounted", GUI_BAD, GUI_PANEL);
        return;
    }

    text_px(x + 10, y + 30, "Name", GUI_MUTED, GUI_PANEL);
    text_px(x + 210, y + 30, "Type", GUI_MUTED, GUI_PANEL);
    text_px(x + 300, y + 30, "Size", GUI_MUTED, GUI_PANEL);
    text_px(x + 390, y + 30, "Perm", GUI_MUTED, GUI_PANEL);
    if (w > 16) {
        fb_fill_rect(x + 8, y + 44, w - 16, 1, GUI_BORDER);
    }

    uint32_t max_rows = (h > 64) ? (h - 64) / LINE_H : 0;
    if (max_rows > 18) {
        max_rows = 18;
    }

    uint32_t row = 0;
    for (uint32_t i = 0; row < max_rows; i++) {
        struct dirent *entry = vfs_readdir(root, i);
        if (!entry) {
            break;
        }
        struct vfs_node *node = vfs_finddir(root, entry->name);
        uint32_t ry = y + 54 + (row * LINE_H);
        uint32_t bg = (row % 2 == 0) ? GUI_PANEL : GUI_PANEL_ALT;
        if (w > 12) {
            fb_fill_rect(x + 6, ry - 3, w - 12, LINE_H, bg);
        }
        clipped_text(x + 10, ry, 22, entry->name, GUI_TEXT, bg);
        text_px(x + 210, ry, (node && vfs_is_directory(node)) ? "dir" : "file",
                GUI_MUTED, bg);
        if (node) {
            char size[32];
            ksnprintf(size, sizeof(size), "%llu", vfs_size(node));
            text_px(x + 300, ry, size, GUI_TEXT, bg);
            char perm[8];
            perm[0] = vfs_can_read(node) ? 'r' : '-';
            perm[1] = vfs_can_write(node) ? 'w' : '-';
            perm[2] = vfs_can_exec(node) ? 'x' : '-';
            perm[3] = '\0';
            text_px(x + 390, ry, perm, GUI_TEXT, bg);
        }
        row++;
    }

    if (row == 0) {
        text_px(x + 10, y + 58, "Directory is empty", GUI_MUTED, GUI_PANEL);
    }
}

static void draw_system(void) {
    uint32_t x = content_x();
    uint32_t y = content_y();
    uint32_t w = content_w();
    uint32_t h = content_h();
    draw_panel(x, y, w, h, "System Health");

    uint32_t col2 = x + (w / 2);
    label_mb(x + 12, y + 34, "Memory total", pmm_get_total_memory());
    label_mb(x + 12, y + 50, "Memory used", pmm_get_used_memory());
    label_mb(x + 12, y + 66, "Memory free", pmm_get_free_memory());
    label_u64(x + 12, y + 82, "Heap used", heap_get_used());
    label_u64(x + 12, y + 98, "Heap free", heap_get_free());

    label_u64(col2, y + 34, "CPU entries", smp_cpu_count());
    label_u64(col2, y + 50, "CPU enabled", smp_enabled_cpu_count());
    label_u64(col2, y + 66, "CPU online", smp_online_cpu_count());
    label_value(col2, y + 82, "AP startup", smp_ap_startup_supported() ? "supported" : "not active");
    label_value(col2, y + 98, "MADT", acpi_madt_available() ? "present" : "missing");
}

static void draw_drivers(void) {
    uint32_t x = content_x();
    uint32_t y = content_y();
    uint32_t w = content_w();
    uint32_t h = content_h();
    draw_panel(x, y, w, h, "Hardware Probe Status");

    struct pci_probe_summary summary;
    pci_get_probe_summary(&summary);

    label_u64(x + 12, y + 34, "PCI devices", summary.devices);
    label_u64(x + 12, y + 50, "Probe drivers", summary.driver_count);
    label_u64(x + 12, y + 66, "Probe failures", summary.failures);

    text_px(x + 12, y + 96, "AHCI", GUI_TEXT, GUI_PANEL);
    text_px(x + 120, y + 96, summary.ahci_matches ? "detected" : "not found",
            summary.ahci_matches ? GUI_WARN : GUI_MUTED, GUI_PANEL);
    text_px(x + 12, y + 112, "NVMe", GUI_TEXT, GUI_PANEL);
    text_px(x + 120, y + 112, summary.nvme_matches ? "detected" : "not found",
            summary.nvme_matches ? GUI_WARN : GUI_MUTED, GUI_PANEL);
    text_px(x + 12, y + 128, "USB", GUI_TEXT, GUI_PANEL);
    text_px(x + 120, y + 128, summary.usb_matches ? "detected" : "not found",
            summary.usb_matches ? GUI_WARN : GUI_MUTED, GUI_PANEL);
    text_px(x + 12, y + 144, "Network", GUI_TEXT, GUI_PANEL);
    text_px(x + 120, y + 144, summary.net_matches ? "detected" : "not found",
            summary.net_matches ? GUI_WARN : GUI_MUTED, GUI_PANEL);

    text_px(x + 12, y + 178, "Storage and network stacks are still probe-only.", GUI_WARN, GUI_PANEL);
    text_px(x + 12, y + 194, "Serial logs contain per-device BAR and capability detail.", GUI_MUTED, GUI_PANEL);
}

static void draw_processes(void) {
    uint32_t x = content_x();
    uint32_t y = content_y();
    uint32_t w = content_w();
    uint32_t h = content_h();
    draw_panel(x, y, w, h, "Process Monitor");

    label_u64(x + 12, y + 34, "Process count", process_count());
    label_u64(x + 12, y + 50, "Scheduler switches", scheduler_get_switches());

    struct process *current = process_current();
    if (current) {
        label_u64(x + 12, y + 74, "Current PID", current->pid);
        label_value(x + 12, y + 90, "Current name", current->name);
        label_u64(x + 12, y + 106, "Current UID", current->uid);
    } else {
        text_px(x + 12, y + 74, "No current process selected", GUI_MUTED, GUI_PANEL);
    }
}

static void draw_users(void) {
    uint32_t x = content_x();
    uint32_t y = content_y();
    uint32_t w = content_w();
    uint32_t h = content_h();
    draw_panel(x, y, w, h, "Users");

    label_value(x + 12, y + 34, "Current", user_get_current_name());
    label_u64(x + 12, y + 50, "UID", user_get_current_uid());
    label_value(x + 12, y + 66, "Role", user_is_admin() ? "admin" : "user");
    label_u64(x + 12, y + 82, "Loaded users", (uint64_t)user_count_users());

    uint32_t max_rows = (h > 124) ? (h - 124) / LINE_H : 0;
    if (max_rows > 12) {
        max_rows = 12;
    }

    for (uint32_t i = 0; i < max_rows; i++) {
        const User *u = user_get_by_index((int)i);
        if (!u) {
            break;
        }
        char buf[96];
        ksnprintf(buf, sizeof(buf), "%u  %s  uid=%u  %s",
                  i + 1, u->username, u->uid, u->is_admin ? "admin" : "user");
        text_px(x + 12, y + 112 + (i * LINE_H), buf,
                u->is_active ? GUI_TEXT : GUI_MUTED, GUI_PANEL);
    }
}

static void gui_draw(enum gui_view active) {
    fb_fill_rect(0, 0, screen_w(), screen_h(), GUI_BG);
    draw_topbar(active);
    draw_sidebar(active);
    draw_footer();

    switch (active) {
        case GUI_VIEW_FILES:
            draw_files();
            break;
        case GUI_VIEW_SYSTEM:
            draw_system();
            break;
        case GUI_VIEW_DRIVERS:
            draw_drivers();
            break;
        case GUI_VIEW_PROCESSES:
            draw_processes();
            break;
        case GUI_VIEW_USERS:
            draw_users();
            break;
        default:
            break;
    }
}

void gui_run(void) {
    enum gui_view active = GUI_VIEW_FILES;
    gui_draw(active);

    while (1) {
        if (!khaschar()) {
            __asm__ volatile ("hlt");
            continue;
        }

        char c = kgetc();
        if (c == 'q' || c == 'Q' || c == 27) {
            break;
        }
        if (c >= '1' && c <= '5') {
            active = (enum gui_view)(c - '1');
        }
        gui_draw(active);
    }

    fb_clear();
}
