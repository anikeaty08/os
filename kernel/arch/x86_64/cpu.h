/*
 * AstraOS - CPU Control Header
 * Architecture-independent CPU control interface
 */

#ifndef _ASTRA_ARCH_CPU_H
#define _ASTRA_ARCH_CPU_H

#include <stdint.h>
#include <stdbool.h>

/*
 * cpu_cli - Disable interrupts
 */
static inline void cpu_cli(void) {
    __asm__ volatile ("cli" ::: "memory");
}

/*
 * cpu_sti - Enable interrupts
 */
static inline void cpu_sti(void) {
    __asm__ volatile ("sti" ::: "memory");
}

/*
 * cpu_hlt - Halt CPU until next interrupt
 */
static inline void cpu_hlt(void) {
    __asm__ volatile ("hlt");
}

/*
 * cpu_pause - Pause CPU (for spinlocks)
 */
static inline void cpu_pause(void) {
    __asm__ volatile ("pause");
}

/*
 * cpu_save_flags - Save interrupt flag state
 * Returns current flags register
 */
static inline uint64_t cpu_save_flags(void) {
    uint64_t flags;
    __asm__ volatile (
        "pushfq\n\t"
        "pop %0"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

/*
 * cpu_restore_flags - Restore interrupt flag state
 */
static inline void cpu_restore_flags(uint64_t flags) {
    __asm__ volatile (
        "push %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

/*
 * cpu_interrupts_enabled - Check if interrupts are enabled
 */
static inline bool cpu_interrupts_enabled(void) {
    uint64_t flags = cpu_save_flags();
    return (flags & 0x200) != 0;  /* IF flag is bit 9 */
}

/*
 * cpu_read_cr0 - Read CR0 register
 */
static inline uint64_t cpu_read_cr0(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

/*
 * cpu_write_cr0 - Write CR0 register
 */
static inline void cpu_write_cr0(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(value));
}

/*
 * cpu_read_cr2 - Read CR2 register (page fault address)
 */
static inline uint64_t cpu_read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

/*
 * cpu_read_cr3 - Read CR3 register (page table base)
 */
static inline uint64_t cpu_read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

/*
 * cpu_write_cr3 - Write CR3 register (switch page table)
 */
static inline void cpu_write_cr3(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
}

/*
 * cpu_read_cr4 - Read CR4 register
 */
static inline uint64_t cpu_read_cr4(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}

/*
 * cpu_write_cr4 - Write CR4 register
 */
static inline void cpu_write_cr4(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(value));
}

/*
 * cpu_invlpg - Invalidate TLB entry for virtual address
 */
static inline void cpu_invlpg(uint64_t addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

/*
 * cpu_rdmsr - Read Model Specific Register
 */
static inline uint64_t cpu_rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/*
 * cpu_wrmsr - Write Model Specific Register
 */
static inline void cpu_wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

/*
 * cpu_halt_forever - Halt CPU permanently (for panic)
 */
static inline void cpu_halt_forever(void) {
    cpu_cli();
    for (;;) {
        cpu_hlt();
    }
}

#endif /* _ASTRA_ARCH_CPU_H */
