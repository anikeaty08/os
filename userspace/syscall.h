#ifndef ASTRA_USER_SYSCALL_H
#define ASTRA_USER_SYSCALL_H

#include <stddef.h>
#include <stdint.h>

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_GETPID 2
#define SYS_YIELD  3
#define SYS_READ   4
#define SYS_OPEN   5
#define SYS_CLOSE  6

static inline long syscall3(long number, long a0, long a1, long a2) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(number), "D"(a0), "S"(a1), "d"(a2)
        : "memory"
    );
    return ret;
}

static inline long sys_read(int fd, void *buf, size_t len) {
    return syscall3(SYS_READ, fd, (long)buf, len);
}

static inline long sys_open(const char *path) {
    return syscall3(SYS_OPEN, (long)path, 0, 0);
}

static inline long sys_close(int fd) {
    return syscall3(SYS_CLOSE, fd, 0, 0);
}

static inline long sys_write(int fd, const void *buf, size_t len) {
    return syscall3(SYS_WRITE, fd, (long)buf, len);
}

static inline long sys_getpid(void) {
    return syscall3(SYS_GETPID, 0, 0, 0);
}

static inline void sys_yield(void) {
    (void)syscall3(SYS_YIELD, 0, 0, 0);
}

__attribute__((noreturn))
static inline void sys_exit(int code) {
    (void)syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {
        __asm__ volatile ("pause");
    }
}

#endif /* ASTRA_USER_SYSCALL_H */
