/*
 * AstraOS - Interrupt Service Routines Header
 * Exception and interrupt handling
 */

#ifndef _ASTRA_ARCH_ISR_H
#define _ASTRA_ARCH_ISR_H

#include <stdint.h>
#include "idt.h"

/*
 * Exception names for debugging
 */
extern const char *exception_names[32];

/*
 * Main ISR handler called from assembly
 * Dispatches to appropriate handler based on interrupt number
 */
void isr_handler(struct interrupt_frame *frame);

/*
 * Register all ISR stubs with the IDT
 */
void isr_install(void);

/*
 * ISR stub table (defined in assembly)
 */
extern void *isr_stub_table[];

#endif /* _ASTRA_ARCH_ISR_H */
