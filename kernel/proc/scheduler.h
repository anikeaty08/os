/*
 * AstraOS - Scheduler Header
 * Round-robin process scheduler
 */

#ifndef _ASTRA_PROC_SCHEDULER_H
#define _ASTRA_PROC_SCHEDULER_H

#include "process.h"

/*
 * Initialize scheduler
 */
void scheduler_init(void);

/*
 * Add process to ready queue
 */
void scheduler_add(struct process *proc);

/*
 * Remove process from ready queue
 */
void scheduler_remove(struct process *proc);

/*
 * Main scheduling function
 * Called from non-IRQ context only!
 */
void schedule(void);

/*
 * Timer tick handler
 * Called from timer IRQ to decrement time slices
 * Returns true if reschedule needed
 */
bool scheduler_tick(void);

/*
 * Get scheduler statistics
 */
uint64_t scheduler_get_switches(void);

#endif /* _ASTRA_PROC_SCHEDULER_H */
