/*
 * AstraOS - PIT Timer Driver Implementation
 * Programmable Interval Timer (8253/8254)
 *
 * IMPORTANT: The IRQ handler is lightweight per design constraints.
 * It only increments the tick counter and sets a flag.
 * Actual scheduling is done outside IRQ context.
 */

#include "pit.h"
#include "../arch/x86_64/io.h"
#include "../arch/x86_64/irq.h"

/*
 * Tick counter - volatile because modified in IRQ
 */
static volatile uint64_t ticks = 0;

/*
 * Reschedule flag - set in IRQ, checked in main loop
 */
static volatile bool need_reschedule = false;

/*
 * Reschedule callback (optional)
 */
static reschedule_callback_t reschedule_callback = NULL;

/*
 * Current frequency
 */
static uint32_t current_frequency = 0;

/*
 * Timer IRQ handler - MUST BE FAST!
 * Only increments counter and sets flag
 */
static void pit_irq_handler(uint8_t irq) {
    (void)irq;

    /* Increment tick counter */
    ticks++;

    /* Set reschedule flag periodically (every 10ms for 1000Hz timer) */
    if ((ticks % 10) == 0) {
        need_reschedule = true;
    }
}

/*
 * Initialize PIT
 */
void pit_init(uint32_t frequency) {
    /* Calculate divisor */
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;

    current_frequency = PIT_FREQUENCY / divisor;

    /* Set command byte:
     * Bits 7-6: Channel 0
     * Bits 5-4: Access mode lobyte/hibyte
     * Bits 3-1: Mode 3 (square wave)
     * Bit 0: Binary mode
     */
    outb(PIT_COMMAND, 0x36);

    /* Send divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);         /* Low byte */
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);  /* High byte */

    /* Register IRQ handler */
    irq_register(IRQ_TIMER, pit_irq_handler);

    /* Enable timer IRQ */
    irq_enable(IRQ_TIMER);
}

/*
 * Get current tick count
 */
uint64_t pit_get_ticks(void) {
    return ticks;
}

/*
 * Sleep for specified milliseconds
 * Note: This is a busy-wait sleep, suitable for short delays
 */
void pit_sleep_ms(uint64_t ms) {
    uint64_t target = ticks + (ms * current_frequency / 1000);
    while (ticks < target) {
        __asm__ volatile ("hlt");
    }
}

/*
 * Check if rescheduling is needed
 */
bool pit_check_reschedule(void) {
    return need_reschedule;
}

/*
 * Clear reschedule flag
 */
void pit_clear_reschedule(void) {
    need_reschedule = false;
}

/*
 * Set reschedule callback
 */
void pit_set_reschedule_callback(reschedule_callback_t callback) {
    reschedule_callback = callback;
}
