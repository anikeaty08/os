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
#define SYS_SPAWN  7
#define SYS_KILL   8
#define SYS_WAIT   9
#define SYS_CREATE 10
#define SYS_UNLINK 11
#define SYS_RENAME 12
#define SYS_TRUNCATE 13
#define SYS_FSCK   14

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

static inline long sys_create(const char *path) {
    return syscall3(SYS_CREATE, (long)path, 0, 0);
}

static inline long sys_unlink(const char *path) {
    return syscall3(SYS_UNLINK, (long)path, 0, 0);
}

static inline long sys_rename(const char *old_path, const char *new_path) {
    return syscall3(SYS_RENAME, (long)old_path, (long)new_path, 0);
}

static inline long sys_truncate(const char *path, uint64_t size) {
    return syscall3(SYS_TRUNCATE, (long)path, (long)size, 0);
}

static inline long sys_fsck(int repair) {
    return syscall3(SYS_FSCK, repair ? 1 : 0, 0, 0);
}

static inline long sys_spawn(const char *path) {
    return syscall3(SYS_SPAWN, (long)path, 0, 0);
}

static inline long sys_kill(long pid) {
    return syscall3(SYS_KILL, pid, 0, 0);
}

static inline long sys_wait(long pid, int *status) {
    return syscall3(SYS_WAIT, pid, (long)status, 0);
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
