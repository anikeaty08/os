/*
 * AstraOS - 8259 PIC Driver Implementation
 * Programmable Interrupt Controller management
 */

#include "pic.h"
#include "io.h"

/*
 * Current IRQ masks
 */
static uint8_t pic1_mask = 0xFF;  /* All masked initially */
static uint8_t pic2_mask = 0xFF;

/*
 * Remap the PIC IRQs to new vector offsets
 * This is necessary because the default mapping (0-15) conflicts
 * with CPU exception vectors (0-31)
 */
void pic_remap(uint8_t offset1, uint8_t offset2) {
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (cascade mode) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, offset1);  /* Master PIC vector offset */
    io_wait();
    outb(PIC2_DATA, offset2);  /* Slave PIC vector offset */
    io_wait();

    /* ICW3: Configure cascade */
    outb(PIC1_DATA, 4);        /* Tell Master that Slave is at IRQ2 (0000 0100) */
    io_wait();
    outb(PIC2_DATA, 2);        /* Tell Slave its cascade identity (0000 0010) */
    io_wait();

    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    pic1_mask = mask1;
    pic2_mask = mask2;
}

/*
 * Initialize PICs with default settings
 */
void pic_init(void) {
    /* Remap IRQs: 0-7 -> 32-39, 8-15 -> 40-47 */
    pic_remap(PIC1_OFFSET, PIC2_OFFSET);

    /* Mask all IRQs initially (will be unmasked as drivers register) */
    pic_disable_all();
}

/*
 * Send End of Interrupt
 */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        /* IRQ came from slave PIC, need to send EOI to both */
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

/*
 * Enable (unmask) a specific IRQ
 */
void pic_enable_irq(uint8_t irq) {
    if (irq < 8) {
        pic1_mask &= ~(1 << irq);
        outb(PIC1_DATA, pic1_mask);
    } else {
        pic2_mask &= ~(1 << (irq - 8));
        outb(PIC2_DATA, pic2_mask);

        /* Also unmask IRQ2 (cascade) on master if not already */
        if (pic1_mask & (1 << 2)) {
            pic1_mask &= ~(1 << 2);
            outb(PIC1_DATA, pic1_mask);
        }
    }
}

/*
 * Disable (mask) a specific IRQ
 */
void pic_disable_irq(uint8_t irq) {
    if (irq < 8) {
        pic1_mask |= (1 << irq);
        outb(PIC1_DATA, pic1_mask);
    } else {
        pic2_mask |= (1 << (irq - 8));
        outb(PIC2_DATA, pic2_mask);
    }
}

/*
 * Disable all IRQs
 */
void pic_disable_all(void) {
    pic1_mask = 0xFF;
    pic2_mask = 0xFF;
    outb(PIC1_DATA, pic1_mask);
    outb(PIC2_DATA, pic2_mask);
}

/*
 * Get Interrupt Request Register
 */
uint16_t pic_get_irr(void) {
    outb(PIC1_COMMAND, PIC_READ_IRR);
    outb(PIC2_COMMAND, PIC_READ_IRR);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/*
 * Get In-Service Register
 */
uint16_t pic_get_isr(void) {
    outb(PIC1_COMMAND, PIC_READ_ISR);
    outb(PIC2_COMMAND, PIC_READ_ISR);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/*
 * Check for spurious IRQ
 * Spurious IRQs can occur on IRQ7 (master) or IRQ15 (slave)
 */
int pic_is_spurious(uint8_t irq) {
    if (irq == 7) {
        /* Check if IRQ7 is real by reading ISR */
        outb(PIC1_COMMAND, PIC_READ_ISR);
        if ((inb(PIC1_COMMAND) & 0x80) == 0) {
            /* Spurious - don't send EOI */
            return 1;
        }
    } else if (irq == 15) {
        /* Check if IRQ15 is real */
        outb(PIC2_COMMAND, PIC_READ_ISR);
        if ((inb(PIC2_COMMAND) & 0x80) == 0) {
            /* Spurious - send EOI only to master (for IRQ2 cascade) */
            outb(PIC1_COMMAND, PIC_EOI);
            return 1;
        }
    }
    return 0;
}
