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
#include "../../mm/heap.h"
#include "../../fs/vfs.h"

#define USER_PATH_MAX 256
#define USER_EXEC_MAX (1024 * 1024)

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
    if (len == 0) {
        return 0;
    }

    if (!user_range_valid((uint64_t)buf, len)) {
        return -1;
    }

    if (fd != 1 && fd != 2) {
        struct process *current = process_current();
        return process_fd_write(current, (int)fd, (const uint8_t *)buf, len);
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
    if (len == 0) {
        return 0;
    }

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
    struct process *current = process_current();
    if (!node || vfs_is_directory(node) ||
        !vfs_can_read_as(node, current ? current->uid : 0,
                         current ? current->is_admin : true)) {
        if (node) vfs_close(node);
        return -1;
    }

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

static int64_t sys_create(const char *path) {
    char kernel_path[USER_PATH_MAX];

    if (copy_user_string((uint64_t)path, kernel_path, sizeof(kernel_path)) < 0) {
        return -1;
    }

    struct process *current = process_current();
    struct vfs_node *node = vfs_create(kernel_path, current ? current->uid : 0);
    if (!node || vfs_is_directory(node)) {
        return -1;
    }

    if (!vfs_can_write_as(node, current ? current->uid : 0,
                          current ? current->is_admin : true)) {
        vfs_close(node);
        return -1;
    }

    int fd = process_fd_open(current, node);
    if (fd < 0) {
        vfs_close(node);
        return -1;
    }

    return fd;
}

static int64_t sys_unlink(const char *path) {
    char kernel_path[USER_PATH_MAX];

    if (copy_user_string((uint64_t)path, kernel_path, sizeof(kernel_path)) < 0) {
        return -1;
    }

    struct process *current = process_current();
    struct vfs_node *node = vfs_resolve_path(kernel_path);
    if (!node ||
        !vfs_can_write_as(node, current ? current->uid : 0,
                          current ? current->is_admin : true)) {
        return -1;
    }

    return vfs_unlink(kernel_path);
}

static int64_t sys_rename(const char *old_path, const char *new_path) {
    char kernel_old[USER_PATH_MAX];
    char kernel_new[USER_PATH_MAX];

    if (copy_user_string((uint64_t)old_path, kernel_old, sizeof(kernel_old)) < 0 ||
        copy_user_string((uint64_t)new_path, kernel_new, sizeof(kernel_new)) < 0) {
        return -1;
    }

    struct process *current = process_current();
    struct vfs_node *node = vfs_resolve_path(kernel_old);
    if (!node ||
        !vfs_can_write_as(node, current ? current->uid : 0,
                          current ? current->is_admin : true)) {
        return -1;
    }

    return vfs_rename(kernel_old, kernel_new);
}

static int64_t sys_truncate(const char *path, uint64_t size) {
    char kernel_path[USER_PATH_MAX];

    if (copy_user_string((uint64_t)path, kernel_path, sizeof(kernel_path)) < 0) {
        return -1;
    }

    struct process *current = process_current();
    struct vfs_node *node = vfs_resolve_path(kernel_path);
    if (!node || vfs_is_directory(node) ||
        !vfs_can_write_as(node, current ? current->uid : 0,
                          current ? current->is_admin : true)) {
        return -1;
    }

    return vfs_truncate(node, size);
}

static int64_t sys_fsck(uint64_t repair) {
    struct process *current = process_current();
    if (current && !current->is_admin) {
        return -1;
    }

    return vfs_fsck(repair ? VFS_FSCK_REPAIR : 0);
}

static int64_t sys_spawn(const char *path) {
    char kernel_path[USER_PATH_MAX];

    if (copy_user_string((uint64_t)path, kernel_path, sizeof(kernel_path)) < 0) {
        return -1;
    }

    struct vfs_node *node = vfs_open(kernel_path);
    struct process *current = process_current();
    if (!node || vfs_is_directory(node) ||
        !vfs_can_exec_as(node, current ? current->uid : 0,
                         current ? current->is_admin : true)) {
        if (node) vfs_close(node);
        return -1;
    }

    uint64_t size = vfs_size(node);
    if (size == 0 || size > USER_EXEC_MAX) {
        vfs_close(node);
        return -1;
    }

    uint8_t *image = kmalloc((size_t)size);
    if (!image) {
        vfs_close(node);
        return -1;
    }

    uint64_t offset = 0;
    while (offset < size) {
        size_t to_read = 4096;
        if (to_read > size - offset) {
            to_read = (size_t)(size - offset);
        }

        int bytes = vfs_read(node, offset, to_read, image + offset);
        if (bytes <= 0) {
            kfree(image);
            vfs_close(node);
            return -1;
        }

        offset += (uint64_t)bytes;
    }

    vfs_close(node);

    struct process *child = process_create_elf(kernel_path, image, (size_t)size);
    kfree(image);

    if (!child) {
        return -1;
    }

    return (int64_t)child->pid;
}

static int64_t sys_wait(uint64_t pid, int *status) {
    int kernel_status = 0;
    int result;

    if (status && !user_range_writable((uint64_t)status, sizeof(*status))) {
        return -1;
    }

    result = process_wait(pid, status ? &kernel_status : NULL);
    if (result < 0) {
        return result;
    }

    if (status) {
        *status = kernel_status;
    }

    return result;
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

        case SYS_CREATE:
            result = sys_create((const char *)frame->rdi);
            break;

        case SYS_UNLINK:
            result = sys_unlink((const char *)frame->rdi);
            break;

        case SYS_RENAME:
            result = sys_rename((const char *)frame->rdi,
                                (const char *)frame->rsi);
            break;

        case SYS_TRUNCATE:
            result = sys_truncate((const char *)frame->rdi, frame->rsi);
            break;

        case SYS_FSCK:
            result = sys_fsck(frame->rdi);
            break;

        case SYS_SPAWN:
            result = sys_spawn((const char *)frame->rdi);
            break;

        case SYS_KILL:
            result = process_kill(frame->rdi);
            break;

        case SYS_WAIT:
            result = sys_wait(frame->rdi, (int *)frame->rsi);
            break;

        default:
            result = -1;
            break;
    }

    frame->rax = (uint64_t)result;
}
