/*
 * AstraOS - Interrupt Descriptor Table Implementation
 * Sets up the IDT for x86_64
 */

#include "idt.h"
#include "isr.h"
#include "gdt.h"
#include "../../lib/string.h"

/*
 * The IDT itself
 */
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_pointer idtr;

/* External assembly function to load IDT */
extern void idt_load(struct idt_pointer *idtr);

/*
 * Set an IDT entry
 */
void idt_set_entry(uint8_t vector, void *handler, uint8_t type_attr, uint8_t ist) {
    uint64_t addr = (uint64_t)handler;

    idt[vector].offset_low  = addr & 0xFFFF;
    idt[vector].selector    = GDT_KERNEL_CODE_SELECTOR;
    idt[vector].ist         = ist & 0x07;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved    = 0;
}

/*
 * Convenience function to set a standard interrupt handler
 */
void idt_set_handler(uint8_t vector, void *handler) {
    idt_set_entry(vector, handler,
        IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_TYPE_INTERRUPT,
        0  /* No IST */
    );
}

/*
 * Initialize the IDT
 */
void idt_init(void) {
    /* Clear the IDT */
    memset(idt, 0, sizeof(idt));

    /* Install exception and IRQ handlers */
    isr_install();

    /* Set up IDT pointer */
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    /* Load the IDT */
    idt_load(&idtr);
}
