/*
 * AstraOS - Spinlock Implementation
 * Simple spinlock for kernel synchronization
 */

#include "spinlock.h"
#include "../arch/x86_64/cpu.h"

/*
 * Initialize spinlock
 */
void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

/*
 * Acquire spinlock using atomic exchange
 */
void spinlock_acquire(spinlock_t *lock) {
    while (1) {
        /* Try to acquire with atomic exchange */
        if (__sync_lock_test_and_set(&lock->locked, 1) == 0) {
            /* Memory barrier */
            __sync_synchronize();
            return;
        }

        /* Spin while locked, using PAUSE to reduce power and contention */
        while (lock->locked) {
            cpu_pause();
        }
    }
}

/*
 * Release spinlock
 */
void spinlock_release(spinlock_t *lock) {
    /* Memory barrier before release */
    __sync_synchronize();
    __sync_lock_release(&lock->locked);
}

/*
 * Try to acquire (non-blocking)
 */
int spinlock_try_acquire(spinlock_t *lock) {
    if (__sync_lock_test_and_set(&lock->locked, 1) == 0) {
        __sync_synchronize();
        return 1;
    }
    return 0;
}

/*
 * Acquire with interrupt disable
 */
void spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags) {
    /* Save and disable interrupts first */
    *flags = cpu_save_flags();
    cpu_cli();

    /* Then acquire the lock */
    spinlock_acquire(lock);
}

/*
 * Release with interrupt restore
 */
void spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags) {
    /* Release lock first */
    spinlock_release(lock);

    /* Then restore interrupts */
    cpu_restore_flags(flags);
}
