/*
 * AstraOS - IRQ Abstraction Layer Header
 * Hardware-independent interrupt request interface
 *
 * This abstraction allows switching between PIC and APIC
 * without changing driver code
 */

#ifndef _ASTRA_ARCH_IRQ_H
#define _ASTRA_ARCH_IRQ_H

#include <stdint.h>

/*
 * Maximum number of IRQs
 */
#define IRQ_MAX         16

/*
 * Standard PC IRQ assignments
 */
#define IRQ_TIMER       0   /* PIT Timer */
#define IRQ_KEYBOARD    1   /* PS/2 Keyboard */
#define IRQ_CASCADE     2   /* Cascade (used internally) */
#define IRQ_COM2        3   /* Serial Port 2 */
#define IRQ_COM1        4   /* Serial Port 1 */
#define IRQ_LPT2        5   /* Parallel Port 2 / Sound Card */
#define IRQ_FLOPPY      6   /* Floppy Disk */
#define IRQ_LPT1        7   /* Parallel Port 1 (often spurious) */
#define IRQ_RTC         8   /* Real Time Clock */
#define IRQ_ACPI        9   /* ACPI */
#define IRQ_AVAILABLE1  10  /* Available */
#define IRQ_AVAILABLE2  11  /* Available */
#define IRQ_MOUSE       12  /* PS/2 Mouse */
#define IRQ_FPU         13  /* FPU / Coprocessor */
#define IRQ_ATA_PRIMARY 14  /* Primary ATA */
#define IRQ_ATA_SECONDARY 15 /* Secondary ATA (often spurious) */

/*
 * IRQ handler function type
 * Handler receives the IRQ number
 * Handler should be FAST - only acknowledge hardware and queue work
 */
typedef void (*irq_handler_t)(uint8_t irq);

/*
 * Initialize the IRQ subsystem
 * Sets up interrupt controller (PIC or APIC)
 */
void irq_init(void);

/*
 * Register an IRQ handler
 * Returns 0 on success, -1 if already registered
 */
int irq_register(uint8_t irq, irq_handler_t handler);

/*
 * Unregister an IRQ handler
 */
void irq_unregister(uint8_t irq);

/*
 * Enable (unmask) an IRQ
 */
void irq_enable(uint8_t irq);

/*
 * Disable (mask) an IRQ
 */
void irq_disable(uint8_t irq);

/*
 * Send End of Interrupt signal
 * Must be called at end of IRQ handling
 */
void irq_eoi(uint8_t irq);

/*
 * Dispatch an IRQ to its registered handler
 * Called from the ISR common handler
 * Returns 1 if handler was called, 0 if no handler
 */
int irq_dispatch(uint8_t irq);

/*
 * Get IRQ vector number (for IDT)
 * IRQ 0 -> vector 32, etc.
 */
static inline uint8_t irq_to_vector(uint8_t irq) {
    return irq + 32;
}

/*
 * Get IRQ number from vector
 */
static inline uint8_t vector_to_irq(uint8_t vector) {
    return vector - 32;
}

#endif /* _ASTRA_ARCH_IRQ_H */
