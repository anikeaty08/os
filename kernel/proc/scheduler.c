/*
 * AstraOS - Scheduler Implementation
 * Simple round-robin scheduler
 *
 * IMPORTANT: schedule() is called from non-IRQ context only!
 * Timer IRQ only sets a flag, actual scheduling happens here.
 */

#include "scheduler.h"
#include "process.h"
#include "../sync/spinlock.h"
#include "../arch/x86_64/cpu.h"

/*
 * Ready queue (circular linked list)
 */
static struct process *ready_queue_head = NULL;
static struct process *ready_queue_tail = NULL;

/*
 * Scheduler lock
 */
static spinlock_t sched_lock = SPINLOCK_INIT;

/*
 * Statistics
 */
static uint64_t context_switches = 0;

/*
 * Need reschedule flag (set by timer, checked by schedule)
 */
static volatile bool need_reschedule = false;

/*
 * External context switch function
 */
extern void context_switch(struct cpu_context *old, struct cpu_context *new);

/*
 * Initialize scheduler
 */
void scheduler_init(void) {
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    context_switches = 0;
    need_reschedule = false;
}

/*
 * Add process to ready queue
 */
void scheduler_add(struct process *proc) {
    if (!proc || proc->state == PROCESS_UNUSED) return;

    uint64_t flags;
    spinlock_acquire_irqsave(&sched_lock, &flags);

    proc->next = NULL;

    if (ready_queue_tail) {
        ready_queue_tail->next = proc;
        ready_queue_tail = proc;
    } else {
        ready_queue_head = proc;
        ready_queue_tail = proc;
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}

/*
 * Remove process from ready queue
 */
void scheduler_remove(struct process *proc) {
    if (!proc) return;

    uint64_t flags;
    spinlock_acquire_irqsave(&sched_lock, &flags);

    struct process *prev = NULL;
    struct process *curr = ready_queue_head;

    while (curr) {
        if (curr == proc) {
            if (prev) {
                prev->next = curr->next;
            } else {
                ready_queue_head = curr->next;
            }

            if (curr == ready_queue_tail) {
                ready_queue_tail = prev;
            }

            proc->next = NULL;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}

/*
 * Pop next process from ready queue
 */
static struct process *ready_queue_pop(void) {
    if (!ready_queue_head) return NULL;

    struct process *proc = ready_queue_head;
    ready_queue_head = proc->next;

    if (!ready_queue_head) {
        ready_queue_tail = NULL;
    }

    proc->next = NULL;
    return proc;
}

/*
 * Main scheduling function
 * Called from non-IRQ context only!
 */
void schedule(void) {
    uint64_t flags;
    spinlock_acquire_irqsave(&sched_lock, &flags);

    /* Clear reschedule flag */
    need_reschedule = false;

    struct process *current = process_current();

    /* Get next process from ready queue */
    struct process *next = ready_queue_pop();

    /* If no ready process, keep running current or idle */
    if (!next) {
        spinlock_release_irqrestore(&sched_lock, flags);
        return;
    }

    /* If same process, just reset time slice */
    if (next == current) {
        next->time_slice = DEFAULT_TIME_SLICE;
        scheduler_add(next);
        spinlock_release_irqrestore(&sched_lock, flags);
        return;
    }

    /* Put current process back in ready queue if still runnable */
    if (current && current->state == PROCESS_RUNNING) {
        current->state = PROCESS_READY;
        scheduler_add(current);
    }

    /* Switch to next process */
    next->state = PROCESS_RUNNING;
    next->time_slice = DEFAULT_TIME_SLICE;
    process_set_current(next);
    context_switches++;

    /* Perform context switch */
    if (current) {
        spinlock_release_irqrestore(&sched_lock, flags);
        context_switch(&current->context, &next->context);
    } else {
        spinlock_release_irqrestore(&sched_lock, flags);
        /* First switch, just load new context */
        context_switch(NULL, &next->context);
    }
}

/*
 * Timer tick handler
 * Called to check if reschedule is needed
 */
bool scheduler_tick(void) {
    struct process *current = process_current();

    if (current && current->pid != 0) {
        if (current->time_slice > 0) {
            current->time_slice--;
        }

        if (current->time_slice == 0) {
            need_reschedule = true;
            return true;
        }
    }

    return need_reschedule;
}

/*
 * Get context switch count
 */
uint64_t scheduler_get_switches(void) {
    return context_switches;
}
