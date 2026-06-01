/*
 * AstraOS - x86_64 System Call Interface
 *
 * This is the first controlled user/kernel boundary. It intentionally exposes
 * only a very small ABI until ring-3 process loading is complete.
 */

#include "syscall.h"
#include "idt.h"
#include "gdt.h"
#include "../../lib/stdio.h"
#include "../../drivers/serial.h"
#include "../../proc/process.h"
#include "../../mm/vmm.h"
#include "../../fs/vfs.h"

#define USER_PATH_MAX 256

extern void syscall_stub(void);
extern void fb_putchar(char c);

bool user_range_valid(uint64_t ptr, size_t size) {
    if (ptr == 0 || size == 0) return false;
    if (ptr >= USER_SPACE_TOP) return false;
    if (size > USER_SPACE_TOP - ptr) return false;

    struct process *current = process_current();
    if (!current || !current->page_table) return false;

    return vmm_user_range_mapped(current->page_table, ptr, size, false);
}

bool user_range_writable(uint64_t ptr, size_t size) {
    if (ptr == 0 || size == 0) return false;
    if (ptr >= USER_SPACE_TOP) return false;
    if (size > USER_SPACE_TOP - ptr) return false;

    struct process *current = process_current();
    if (!current || !current->page_table) return false;

    return vmm_user_range_mapped(current->page_table, ptr, size, true);
}

void syscall_init(void) {
    idt_set_entry(SYSCALL_VECTOR, syscall_stub,
        IDT_FLAG_PRESENT | IDT_FLAG_DPL3 | IDT_TYPE_TRAP,
        0
    );
}

static int64_t sys_write(uint64_t fd, const char *buf, size_t len) {
    if (fd != 1 && fd != 2) {
        return -1;
    }

    if (!user_range_valid((uint64_t)buf, len)) {
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        serial_putchar(buf[i]);
        if (buf[i] == '\n') {
            fb_putchar('\n');
        } else {
            fb_putchar(buf[i]);
        }
    }

    return (int64_t)len;
}

static int64_t sys_read(uint64_t fd, char *buf, size_t len) {
    if (!user_range_writable((uint64_t)buf, len)) {
        return -1;
    }

    if (fd != 0) {
        struct process *current = process_current();
        return process_fd_read(current, (int)fd, (uint8_t *)buf, len);
    }

    size_t read = 0;
    while (read < len) {
        char c = kgetc();
        buf[read++] = c;
        if (c == '\n') {
            break;
        }
    }

    return (int64_t)read;
}

static int copy_user_string(uint64_t user_ptr, char *out, size_t out_size) {
    if (!out || out_size == 0) return -1;

    for (size_t i = 0; i < out_size; i++) {
        if (!user_range_valid(user_ptr + i, 1)) {
            return -1;
        }

        char c = ((const char *)user_ptr)[i];
        out[i] = c;

        if (c == '\0') {
            return 0;
        }
    }

    out[out_size - 1] = '\0';
    return -1;
}

static int64_t sys_open(const char *path) {
    char kernel_path[USER_PATH_MAX];

    if (copy_user_string((uint64_t)path, kernel_path, sizeof(kernel_path)) < 0) {
        return -1;
    }

    struct vfs_node *node = vfs_open(kernel_path);
    if (!node || vfs_is_directory(node)) {
        if (node) vfs_close(node);
        return -1;
    }

    struct process *current = process_current();
    int fd = process_fd_open(current, node);
    if (fd < 0) {
        vfs_close(node);
        return -1;
    }

    return fd;
}

static int64_t sys_close(uint64_t fd) {
    struct process *current = process_current();
    return process_fd_close(current, (int)fd);
}

void syscall_handler(struct interrupt_frame *frame) {
    if (!frame) return;

    uint64_t number = frame->rax;
    int64_t result = -1;

    switch (number) {
        case SYS_EXIT:
            process_exit((int)frame->rdi);
            result = 0;
            break;

        case SYS_WRITE:
            result = sys_write(frame->rdi, (const char *)frame->rsi,
                               (size_t)frame->rdx);
            break;

        case SYS_GETPID: {
            struct process *current = process_current();
            result = current ? (int64_t)current->pid : -1;
            break;
        }

        case SYS_YIELD:
            process_yield();
            result = 0;
            break;

        case SYS_READ:
            result = sys_read(frame->rdi, (char *)frame->rsi,
                              (size_t)frame->rdx);
            break;

        case SYS_OPEN:
            result = sys_open((const char *)frame->rdi);
            break;

        case SYS_CLOSE:
            result = sys_close(frame->rdi);
            break;

        default:
            result = -1;
            break;
    }

    frame->rax = (uint64_t)result;
}
