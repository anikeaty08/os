/*
 * AstraOS - x86_64 System Call Interface
 */

#ifndef _ASTRA_ARCH_SYSCALL_H
#define _ASTRA_ARCH_SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "idt.h"

#define SYSCALL_VECTOR 0x80

enum syscall_number {
    SYS_EXIT = 0,
    SYS_WRITE = 1,
    SYS_GETPID = 2,
    SYS_YIELD = 3,
};

void syscall_init(void);
void syscall_handler(struct interrupt_frame *frame);

bool user_range_valid(uint64_t ptr, size_t size);

#endif /* _ASTRA_ARCH_SYSCALL_H */
