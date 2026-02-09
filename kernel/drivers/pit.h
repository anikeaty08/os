/*
 * AstraOS - PIT Timer Driver Header
 * Programmable Interval Timer (8253/8254)
 */

#ifndef _ASTRA_DRIVERS_PIT_H
#define _ASTRA_DRIVERS_PIT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * PIT Constants
 */
#define PIT_FREQUENCY   1193182     /* Base oscillator frequency (Hz) */
#define PIT_CHANNEL0    0x40        /* Channel 0 data port */
#define PIT_CHANNEL1    0x41        /* Channel 1 data port */
#define PIT_CHANNEL2    0x42        /* Channel 2 data port */
#define PIT_COMMAND     0x43        /* Command register */

/*
 * Default tick rate (1000 Hz = 1ms per tick)
 */
#define PIT_DEFAULT_HZ  1000

/*
 * Initialize PIT with specified frequency
 */
void pit_init(uint32_t frequency);

/*
 * Get current tick count
 */
uint64_t pit_get_ticks(void);

/*
 * Sleep for specified milliseconds
 */
void pit_sleep_ms(uint64_t ms);

/*
 * Check if rescheduling is needed (called from non-IRQ context)
 */
bool pit_check_reschedule(void);

/*
 * Clear reschedule flag
 */
void pit_clear_reschedule(void);

/*
 * Set reschedule callback (called when time slice expires)
 */
typedef void (*reschedule_callback_t)(void);
void pit_set_reschedule_callback(reschedule_callback_t callback);

#endif /* _ASTRA_DRIVERS_PIT_H */
