/*
 * AstraOS - Process Management Implementation
 * Process creation, destruction, and management
 */

#include "process.h"
#include "scheduler.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../sync/spinlock.h"

/*
 * Process table
 */
static struct process process_table[MAX_PROCESSES];
static uint64_t next_pid = 1;
static struct process *current_process = NULL;
static spinlock_t process_lock = SPINLOCK_INIT;

/*
 * External context switch function (in context.asm)
 */
extern void context_switch(struct cpu_context *old, struct cpu_context *new);

/*
 * Initialize process subsystem
 */
void process_init(void) {
    /* Clear process table */
    memset(process_table, 0, sizeof(process_table));

    /* Create idle/kernel process (PID 0) */
    struct process *idle = &process_table[0];
    idle->pid = 0;
    idle->state = PROCESS_RUNNING;
    idle->cpu_id = 0;
    idle->page_table = vmm_get_kernel_pml4();
    idle->time_slice = DEFAULT_TIME_SLICE;
    strcpy(idle->name, "kernel");

    current_process = idle;
}

/*
 * Find free process slot
 */
static struct process *find_free_slot(void) {
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            return &process_table[i];
        }
    }
    return NULL;
}

/*
 * Process entry wrapper
 * Calls the actual entry point and handles exit
 */
static void process_entry_wrapper(void) {
    /* Entry point is stored in r12 by process_create */
    void (*entry)(void);
    __asm__ volatile ("mov %%r12, %0" : "=r"(entry));

    /* Call the actual entry point */
    if (entry) {
        entry();
    }

    /* Process returned, exit */
    process_exit(0);
}

/*
 * Create a new kernel process
 */
struct process *process_create(const char *name, void (*entry)(void)) {
    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    /* Find free slot */
    struct process *proc = find_free_slot();
    if (!proc) {
        spinlock_release_irqrestore(&process_lock, flags);
        return NULL;
    }

    /* Allocate kernel stack */
    size_t stack_pages = (KERNEL_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    void *stack_phys = pmm_alloc_pages(stack_pages);
    if (!stack_phys) {
        spinlock_release_irqrestore(&process_lock, flags);
        return NULL;
    }

    /* Map kernel stack (use HHDM for simplicity) */
    extern uint64_t hhdm_offset;
    uint64_t stack_base = (uint64_t)stack_phys + hhdm_offset;
    uint64_t stack_top = stack_base + KERNEL_STACK_SIZE;

    /* Initialize process */
    proc->pid = next_pid++;
    proc->state = PROCESS_CREATED;
    proc->cpu_id = 0;
    proc->page_table = vmm_get_kernel_pml4();  /* Share kernel page table */
    proc->kernel_stack = stack_top;
    proc->kernel_stack_base = stack_base;
    proc->user_stack = 0;
    proc->time_slice = DEFAULT_TIME_SLICE;
    proc->exit_code = 0;
    proc->next = NULL;
    proc->parent = current_process;

    if (name) {
        strncpy(proc->name, name, sizeof(proc->name) - 1);
        proc->name[sizeof(proc->name) - 1] = '\0';
    } else {
        strcpy(proc->name, "unnamed");
    }

    /* Set up initial context */
    /* Set up context for context_switch to restore */
    /* context_switch expects: r15, r14, r13, r12, rbp, rbx, rip */
    proc->context.rip = (uint64_t)process_entry_wrapper;
    proc->context.rbx = 0;
    proc->context.rbp = 0;
    proc->context.r12 = (uint64_t)entry;  /* Store entry point in r12 */
    proc->context.r13 = 0;
    proc->context.r14 = 0;
    proc->context.r15 = 0;

    /* Mark as ready and add to scheduler */
    proc->state = PROCESS_READY;
    scheduler_add(proc);

    spinlock_release_irqrestore(&process_lock, flags);
    return proc;
}

/*
 * Exit current process
 */
void process_exit(int exit_code) {
    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    if (current_process && current_process->pid != 0) {
        current_process->exit_code = exit_code;
        current_process->state = PROCESS_ZOMBIE;

        /* Free kernel stack */
        if (current_process->kernel_stack_base) {
            extern uint64_t hhdm_offset;
            void *phys = (void *)(current_process->kernel_stack_base - hhdm_offset);
            size_t pages = (KERNEL_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
            pmm_free_pages(phys, pages);
        }

        /* Mark slot as unused */
        current_process->state = PROCESS_UNUSED;
    }

    spinlock_release_irqrestore(&process_lock, flags);

    /* Schedule next process */
    schedule();

    /* Should never reach here */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/*
 * Get current running process
 */
struct process *process_current(void) {
    return current_process;
}

/*
 * Set current process
 */
void process_set_current(struct process *proc) {
    current_process = proc;
}

/*
 * Get process by PID
 */
struct process *process_get(uint64_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_UNUSED &&
            process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

/*
 * Yield CPU to another process
 */
void process_yield(void) {
    if (current_process) {
        current_process->time_slice = 0;
    }
    schedule();
}

/*
 * Block current process
 */
void process_block(process_state_t reason) {
    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    if (current_process && current_process->pid != 0) {
        current_process->state = reason;
    }

    spinlock_release_irqrestore(&process_lock, flags);
    schedule();
}

/*
 * Unblock a process
 */
void process_unblock(struct process *proc) {
    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    if (proc && proc->state == PROCESS_BLOCKED) {
        proc->state = PROCESS_READY;
        scheduler_add(proc);
    }

    spinlock_release_irqrestore(&process_lock, flags);
}

/*
 * Get process count
 */
uint64_t process_count(void) {
    uint64_t count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_UNUSED) {
            count++;
        }
    }
    return count;
}
