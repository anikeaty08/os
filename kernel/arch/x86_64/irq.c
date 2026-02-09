/*
 * AstraOS - IRQ Abstraction Layer Implementation
 * Hardware-independent interrupt request interface
 */

#include "irq.h"
#include "pic.h"
#include "cpu.h"

/*
 * IRQ handler table
 */
static irq_handler_t irq_handlers[IRQ_MAX] = {0};

/*
 * Initialize IRQ subsystem
 * Currently uses 8259 PIC, can be extended for APIC
 */
void irq_init(void) {
    /* Initialize the 8259 PIC */
    pic_init();
}

/*
 * Register an IRQ handler
 */
int irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq >= IRQ_MAX || handler == NULL) {
        return -1;
    }

    /* Disable interrupts during registration */
    uint64_t flags = cpu_save_flags();
    cpu_cli();

    if (irq_handlers[irq] != NULL) {
        /* Already registered */
        cpu_restore_flags(flags);
        return -1;
    }

    irq_handlers[irq] = handler;

    cpu_restore_flags(flags);
    return 0;
}

/*
 * Unregister an IRQ handler
 */
void irq_unregister(uint8_t irq) {
    if (irq >= IRQ_MAX) {
        return;
    }

    uint64_t flags = cpu_save_flags();
    cpu_cli();

    irq_handlers[irq] = NULL;

    /* Disable the IRQ since no handler */
    pic_disable_irq(irq);

    cpu_restore_flags(flags);
}

/*
 * Enable an IRQ
 */
void irq_enable(uint8_t irq) {
    if (irq >= IRQ_MAX) {
        return;
    }
    pic_enable_irq(irq);
}

/*
 * Disable an IRQ
 */
void irq_disable(uint8_t irq) {
    if (irq >= IRQ_MAX) {
        return;
    }
    pic_disable_irq(irq);
}

/*
 * Send End of Interrupt
 */
void irq_eoi(uint8_t irq) {
    if (irq >= IRQ_MAX) {
        return;
    }
    pic_send_eoi(irq);
}

/*
 * Dispatch IRQ to handler
 * Called from ISR common handler
 */
int irq_dispatch(uint8_t irq) {
    if (irq >= IRQ_MAX) {
        return 0;
    }

    /* Check for spurious IRQ */
    if (pic_is_spurious(irq)) {
        /* Spurious IRQ - don't call handler, PIC already handled */
        return 1;
    }

    irq_handler_t handler = irq_handlers[irq];
    if (handler != NULL) {
        /* Call the handler - it MUST be fast! */
        handler(irq);
        return 1;
    }

    return 0;
}
