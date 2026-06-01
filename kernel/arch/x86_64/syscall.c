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

        default:
            result = -1;
            break;
    }

    frame->rax = (uint64_t)result;
}
