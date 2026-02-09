/*
 * AstraOS - Spinlock Header
 * Simple spinlock for kernel synchronization
 */

#ifndef _ASTRA_SYNC_SPINLOCK_H
#define _ASTRA_SYNC_SPINLOCK_H

#include <stdint.h>

/*
 * Spinlock structure
 */
typedef struct {
    volatile int locked;
} spinlock_t;

/*
 * Static initializer
 */
#define SPINLOCK_INIT { .locked = 0 }

/*
 * Initialize a spinlock
 */
void spinlock_init(spinlock_t *lock);

/*
 * Acquire spinlock (busy-wait)
 */
void spinlock_acquire(spinlock_t *lock);

/*
 * Release spinlock
 */
void spinlock_release(spinlock_t *lock);

/*
 * Try to acquire spinlock (non-blocking)
 * Returns 1 if acquired, 0 if already held
 */
int spinlock_try_acquire(spinlock_t *lock);

/*
 * Acquire spinlock and save interrupt flags
 * Use this when the lock may be accessed from IRQ context
 */
void spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags);

/*
 * Release spinlock and restore interrupt flags
 */
void spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags);

#endif /* _ASTRA_SYNC_SPINLOCK_H */
