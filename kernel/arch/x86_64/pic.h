/*
 * AstraOS - 8259 PIC Driver Header
 * Programmable Interrupt Controller management
 */

#ifndef _ASTRA_ARCH_PIC_H
#define _ASTRA_ARCH_PIC_H

#include <stdint.h>

/*
 * PIC Port Addresses
 */
#define PIC1_COMMAND    0x20    /* Master PIC command port */
#define PIC1_DATA       0x21    /* Master PIC data port */
#define PIC2_COMMAND    0xA0    /* Slave PIC command port */
#define PIC2_DATA       0xA1    /* Slave PIC data port */

/*
 * PIC Commands
 */
#define PIC_EOI         0x20    /* End of Interrupt */
#define PIC_READ_IRR    0x0A    /* Read Interrupt Request Register */
#define PIC_READ_ISR    0x0B    /* Read In-Service Register */

/*
 * ICW1 (Initialization Command Word 1)
 */
#define ICW1_ICW4       0x01    /* ICW4 needed */
#define ICW1_SINGLE     0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 */
#define ICW1_LEVEL      0x08    /* Level triggered mode */
#define ICW1_INIT       0x10    /* Initialization */

/*
 * ICW4 (Initialization Command Word 4)
 */
#define ICW4_8086       0x01    /* 8086/88 mode */
#define ICW4_AUTO       0x02    /* Auto EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode (slave) */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode (master) */
#define ICW4_SFNM       0x10    /* Special fully nested mode */

/*
 * Default IRQ vector offsets
 * We remap IRQs to vectors 32-47 to avoid conflicts with CPU exceptions
 */
#define PIC1_OFFSET     32      /* IRQ 0-7 -> INT 32-39 */
#define PIC2_OFFSET     40      /* IRQ 8-15 -> INT 40-47 */

/*
 * Initialize the 8259 PICs
 * Remaps IRQs to specified vector offsets
 */
void pic_init(void);

/*
 * Remap PIC IRQs to specified vector offsets
 */
void pic_remap(uint8_t offset1, uint8_t offset2);

/*
 * Send End of Interrupt signal
 * Must be called at end of IRQ handler
 */
void pic_send_eoi(uint8_t irq);

/*
 * Enable (unmask) a specific IRQ
 */
void pic_enable_irq(uint8_t irq);

/*
 * Disable (mask) a specific IRQ
 */
void pic_disable_irq(uint8_t irq);

/*
 * Disable all IRQs (mask all)
 */
void pic_disable_all(void);

/*
 * Get the combined IRR (Interrupt Request Register)
 */
uint16_t pic_get_irr(void);

/*
 * Get the combined ISR (In-Service Register)
 */
uint16_t pic_get_isr(void);

/*
 * Check if IRQ is spurious (IRQ 7 or 15)
 * Returns 1 if spurious, 0 if real
 */
int pic_is_spurious(uint8_t irq);

#endif /* _ASTRA_ARCH_PIC_H */
