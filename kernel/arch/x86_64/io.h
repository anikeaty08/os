/*
 * AstraOS - x86_64 Port I/O Header
 * Low-level port I/O operations
 */

#ifndef _ASTRA_ARCH_IO_H
#define _ASTRA_ARCH_IO_H

#include <stdint.h>
#include <stddef.h>

/*
 * outb - Write byte to I/O port
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * inb - Read byte from I/O port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * outw - Write word (16-bit) to I/O port
 */
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * inw - Read word (16-bit) from I/O port
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * outl - Write long (32-bit) to I/O port
 */
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * inl - Read long (32-bit) from I/O port
 */
static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * io_wait - Wait for I/O operation to complete
 * Uses an unused port (0x80) for timing
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/*
 * insw - Read multiple words from I/O port
 */
static inline void insw(uint16_t port, void *addr, size_t count) {
    __asm__ volatile (
        "rep insw"
        : "+D"(addr), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

/*
 * outsw - Write multiple words to I/O port
 */
static inline void outsw(uint16_t port, const void *addr, size_t count) {
    __asm__ volatile (
        "rep outsw"
        : "+S"(addr), "+c"(count)
        : "d"(port)
    );
}

#endif /* _ASTRA_ARCH_IO_H */
