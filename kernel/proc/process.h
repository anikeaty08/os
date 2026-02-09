/*
 * AstraOS - Process Management Header
 * Process structures and management functions
 */

#ifndef _ASTRA_PROC_PROCESS_H
#define _ASTRA_PROC_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Process limits
 */
#define MAX_PROCESSES       64
#define KERNEL_STACK_SIZE   (16 * 1024)  /* 16 KB kernel stack per process */
#define USER_STACK_SIZE     (64 * 1024)  /* 64 KB user stack */
#define DEFAULT_TIME_SLICE  10           /* 10 ticks = 10ms at 1000Hz */

/*
 * Process states
 */
typedef enum {
    PROCESS_UNUSED = 0,     /* Slot is free */
    PROCESS_CREATED,        /* Just created, not yet ready */
    PROCESS_READY,          /* Ready to run */
    PROCESS_RUNNING,        /* Currently executing */
    PROCESS_BLOCKED,        /* Waiting for something */
    PROCESS_ZOMBIE          /* Terminated, waiting for cleanup */
} process_state_t;

/*
 * CPU context saved during context switch
 * Must match context.asm layout
 */
struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rip;       /* Return address */
};

/*
 * Process Control Block (PCB)
 */
struct process {
    uint64_t pid;                   /* Process ID */
    process_state_t state;          /* Current state */

    uint64_t cpu_id;                /* CPU affinity (for future SMP) */

    uint64_t *page_table;           /* PML4 for this process */

    uint64_t kernel_stack;          /* Top of kernel stack */
    uint64_t kernel_stack_base;     /* Base of kernel stack (for freeing) */

    uint64_t user_stack;            /* Top of user stack */

    struct cpu_context context;     /* Saved CPU state */

    uint64_t time_slice;            /* Remaining time slice */

    char name[32];                  /* Process name */

    int exit_code;                  /* Exit status */

    struct process *next;           /* Next in ready/wait queue */
    struct process *parent;         /* Parent process */
};

/*
 * Process management functions
 */

/* Initialize process subsystem */
void process_init(void);

/* Create a new kernel process */
struct process *process_create(const char *name, void (*entry)(void));

/* Exit current process */
void process_exit(int exit_code);

/* Get current running process */
struct process *process_current(void);

/* Set current process (used by scheduler) */
void process_set_current(struct process *proc);

/* Get process by PID */
struct process *process_get(uint64_t pid);

/* Yield CPU to another process */
void process_yield(void);

/* Block current process */
void process_block(process_state_t reason);

/* Unblock a process */
void process_unblock(struct process *proc);

/* Get process count */
uint64_t process_count(void);

#endif /* _ASTRA_PROC_PROCESS_H */
