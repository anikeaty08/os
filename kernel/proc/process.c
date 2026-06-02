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
#include "../fs/vfs.h"

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
extern void user_enter(uint64_t entry, uint64_t user_stack);

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

static void user_process_entry_wrapper(void) {
    uint64_t entry;
    uint64_t user_stack;

    __asm__ volatile ("mov %%r12, %0" : "=r"(entry));
    __asm__ volatile ("mov %%r13, %0" : "=r"(user_stack));

    user_enter(entry, user_stack);
    process_exit(-1);
}

static struct process *process_create_user_space(const char *name,
                                                 pagetable_t address_space,
                                                 uint64_t entry,
                                                 uint64_t user_stack) {
    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    struct process *proc = find_free_slot();
    if (!proc) {
        spinlock_release_irqrestore(&process_lock, flags);
        return NULL;
    }

    size_t stack_pages = (KERNEL_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    void *stack_phys = pmm_alloc_pages(stack_pages);
    if (!stack_phys) {
        spinlock_release_irqrestore(&process_lock, flags);
        return NULL;
    }

    extern uint64_t hhdm_offset;
    uint64_t stack_base = (uint64_t)stack_phys + hhdm_offset;
    uint64_t stack_top = stack_base + KERNEL_STACK_SIZE;

    proc->pid = next_pid++;
    proc->state = PROCESS_CREATED;
    proc->cpu_id = 0;
    proc->page_table = address_space;
    proc->kernel_stack = stack_top;
    proc->kernel_stack_base = stack_base;
    proc->user_stack = user_stack;
    proc->time_slice = DEFAULT_TIME_SLICE;
    proc->exit_code = 0;
    proc->wait_observed = false;
    proc->next = NULL;
    proc->parent = current_process;
    memset(proc->files, 0, sizeof(proc->files));

    if (name) {
        strncpy(proc->name, name, sizeof(proc->name) - 1);
        proc->name[sizeof(proc->name) - 1] = '\0';
    } else {
        strcpy(proc->name, "user");
    }

    proc->context.rip = (uint64_t)user_process_entry_wrapper;
    proc->context.rbx = 0;
    proc->context.rbp = 0;
    proc->context.r12 = entry;
    proc->context.r13 = user_stack;
    proc->context.r14 = 0;
    proc->context.r15 = 0;
    proc->context.rsp = stack_top;

    proc->state = PROCESS_READY;
    scheduler_add(proc);

    spinlock_release_irqrestore(&process_lock, flags);
    return proc;
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
    proc->wait_observed = false;
    proc->next = NULL;
    proc->parent = current_process;
    memset(proc->files, 0, sizeof(proc->files));

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
    proc->context.rsp = stack_top;

    /* Mark as ready and add to scheduler */
    proc->state = PROCESS_READY;
    scheduler_add(proc);

    spinlock_release_irqrestore(&process_lock, flags);
    return proc;
}

/*
 * Create a new isolated ring-3 process from a flat code image
 */
struct process *process_create_user(const char *name, const void *image, size_t size) {
    if (!image || size == 0) return NULL;
    if (size > 1024 * 1024) return NULL;

    pagetable_t address_space = vmm_create_address_space();
    if (!address_space) return NULL;

    extern uint64_t hhdm_offset;
    size_t image_pages = PAGE_ALIGN_UP(size) / PAGE_SIZE;
    const uint8_t *src = (const uint8_t *)image;

    for (size_t i = 0; i < image_pages; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            vmm_destroy_address_space(address_space);
            return NULL;
        }

        uint8_t *dst = (uint8_t *)((uint64_t)page + hhdm_offset);
        memset(dst, 0, PAGE_SIZE);

        size_t to_copy = PAGE_SIZE;
        if (i * PAGE_SIZE + to_copy > size) {
            to_copy = size - i * PAGE_SIZE;
        }
        memcpy(dst, src + i * PAGE_SIZE, to_copy);

        if (!vmm_map_page(address_space, USER_ENTRY_VADDR + i * PAGE_SIZE,
                          (uint64_t)page, PTE_USER)) {
            pmm_free_page(page);
            vmm_destroy_address_space(address_space);
            return NULL;
        }
    }

    for (size_t i = 0; i < USER_STACK_SIZE / PAGE_SIZE; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            vmm_destroy_address_space(address_space);
            return NULL;
        }

        uint64_t virt = USER_STACK_TOP - USER_STACK_SIZE + i * PAGE_SIZE;
        if (!vmm_map_page(address_space, virt, (uint64_t)page,
                          PTE_USER | PTE_WRITABLE)) {
            pmm_free_page(page);
            vmm_destroy_address_space(address_space);
            return NULL;
        }
    }

    struct process *proc = process_create_user_space(name, address_space,
                                                     USER_ENTRY_VADDR,
                                                     USER_STACK_TOP);
    if (!proc) {
        vmm_destroy_address_space(address_space);
        return NULL;
    }

    return proc;
}

#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELF_CLASS_64 2
#define ELF_DATA_LSB 1
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_X86_64 0x3E
#define ELF_PH_LOAD 1
#define ELF_PF_X 1
#define ELF_PF_W 2

struct elf64_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

static bool elf_validate_header(const struct elf64_header *eh, size_t size) {
    if (!eh || size < sizeof(*eh)) return false;
    if (eh->ident[0] != ELF_MAGIC0 || eh->ident[1] != ELF_MAGIC1 ||
        eh->ident[2] != ELF_MAGIC2 || eh->ident[3] != ELF_MAGIC3) {
        return false;
    }
    if (eh->ident[4] != ELF_CLASS_64) return false;
    if (eh->ident[5] != ELF_DATA_LSB) return false;
    if (eh->type != ELF_TYPE_EXEC) return false;
    if (eh->machine != ELF_MACHINE_X86_64) return false;
    if (eh->version != 1) return false;
    if (eh->ehsize != sizeof(struct elf64_header)) return false;
    if (eh->phentsize != sizeof(struct elf64_program_header)) return false;
    if (eh->phnum == 0 || eh->phnum > 64) return false;
    if (eh->phoff > size) return false;
    if ((uint64_t)eh->phnum * eh->phentsize > size - eh->phoff) return false;
    if (eh->entry == 0 || eh->entry >= USER_SPACE_TOP) return false;
    return true;
}

static bool elf_range_valid(uint64_t vaddr, uint64_t memsz) {
    if (memsz == 0) return true;
    if (vaddr == 0 || vaddr >= USER_SPACE_TOP) return false;
    if (memsz > USER_SPACE_TOP - vaddr) return false;
    if (vaddr + memsz > USER_STACK_TOP - USER_STACK_SIZE) return false;
    return true;
}

static bool map_elf_segment(pagetable_t address_space,
                            const uint8_t *image,
                            size_t image_size,
                            const struct elf64_program_header *ph) {
    if (ph->memsz < ph->filesz) return false;
    if (ph->offset > image_size) return false;
    if (ph->filesz > image_size - ph->offset) return false;
    if (!elf_range_valid(ph->vaddr, ph->memsz)) return false;

    uint64_t seg_start = PAGE_ALIGN_DOWN(ph->vaddr);
    uint64_t seg_end = PAGE_ALIGN_UP(ph->vaddr + ph->memsz);
    uint64_t map_flags = PTE_USER;

    if (ph->flags & ELF_PF_W) {
        map_flags |= PTE_WRITABLE;
    }
    (void)ELF_PF_X;

    extern uint64_t hhdm_offset;

    for (uint64_t virt = seg_start; virt < seg_end; virt += PAGE_SIZE) {
        if (vmm_virt_to_phys(address_space, virt) != 0) {
            return false;
        }

        void *page = pmm_alloc_page();
        if (!page) return false;

        uint8_t *dst = (uint8_t *)((uint64_t)page + hhdm_offset);
        memset(dst, 0, PAGE_SIZE);

        uint64_t page_start = virt;
        uint64_t page_end = virt + PAGE_SIZE;
        uint64_t file_start = ph->vaddr;
        uint64_t file_end = ph->vaddr + ph->filesz;

        if (page_end > file_start && page_start < file_end) {
            uint64_t copy_start = page_start > file_start ? page_start : file_start;
            uint64_t copy_end = page_end < file_end ? page_end : file_end;
            uint64_t dst_off = copy_start - page_start;
            uint64_t src_off = ph->offset + (copy_start - ph->vaddr);
            memcpy(dst + dst_off, image + src_off, copy_end - copy_start);
        }

        if (!vmm_map_page(address_space, virt, (uint64_t)page, map_flags)) {
            pmm_free_page(page);
            return false;
        }
    }

    return true;
}

/*
 * Create a new isolated ring-3 process from an ELF64 executable image
 */
struct process *process_create_elf(const char *name, const void *image, size_t size) {
    const uint8_t *bytes = (const uint8_t *)image;
    const struct elf64_header *eh = (const struct elf64_header *)image;

    if (!elf_validate_header(eh, size)) return NULL;

    pagetable_t address_space = vmm_create_address_space();
    if (!address_space) return NULL;

    const struct elf64_program_header *ph =
        (const struct elf64_program_header *)(bytes + eh->phoff);
    bool loaded_segment = false;

    for (uint16_t i = 0; i < eh->phnum; i++) {
        if (ph[i].type != ELF_PH_LOAD) {
            continue;
        }

        if (!map_elf_segment(address_space, bytes, size, &ph[i])) {
            vmm_destroy_address_space(address_space);
            return NULL;
        }
        loaded_segment = true;
    }

    if (!loaded_segment) {
        vmm_destroy_address_space(address_space);
        return NULL;
    }

    for (size_t i = 0; i < USER_STACK_SIZE / PAGE_SIZE; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            vmm_destroy_address_space(address_space);
            return NULL;
        }

        uint64_t virt = USER_STACK_TOP - USER_STACK_SIZE + i * PAGE_SIZE;
        if (!vmm_map_page(address_space, virt, (uint64_t)page,
                          PTE_USER | PTE_WRITABLE)) {
            pmm_free_page(page);
            vmm_destroy_address_space(address_space);
            return NULL;
        }
    }

    struct process *proc = process_create_user_space(name, address_space,
                                                     eh->entry,
                                                     USER_STACK_TOP);
    if (!proc) {
        vmm_destroy_address_space(address_space);
        return NULL;
    }

    return proc;
}

/*
 * Exit current process
 */
void process_exit(int exit_code) {
    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    if (current_process && current_process->pid != 0) {
        for (int i = 1; i < MAX_PROCESSES; i++) {
            if (process_table[i].parent == current_process) {
                process_table[i].parent = NULL;
            }
        }

        current_process->exit_code = exit_code;
        current_process->wait_observed = false;
        current_process->state = PROCESS_ZOMBIE;
    }

    spinlock_release_irqrestore(&process_lock, flags);

    /* Schedule next process */
    schedule();

    /* Should never reach here */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

int process_kill(uint64_t pid) {
    if (pid == 0) {
        return -1;
    }

    struct process *current = process_current();
    if (current && current->pid == pid) {
        process_exit(-1);
    }

    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    struct process *proc = process_get(pid);
    if (!proc || proc->state == PROCESS_UNUSED) {
        spinlock_release_irqrestore(&process_lock, flags);
        return -1;
    }

    if (proc->state != PROCESS_ZOMBIE) {
        proc->exit_code = -1;
        proc->wait_observed = false;
        proc->state = PROCESS_ZOMBIE;
        scheduler_remove(proc);
    }

    spinlock_release_irqrestore(&process_lock, flags);
    return 0;
}

int process_wait(uint64_t pid, int *status) {
    struct process *current = process_current();
    if (!current || pid == 0) {
        return -1;
    }

    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    struct process *proc = process_get(pid);
    if (!proc || proc->parent != current || proc->state != PROCESS_ZOMBIE) {
        spinlock_release_irqrestore(&process_lock, flags);
        return -1;
    }

    if (status) {
        *status = proc->exit_code;
    }
    proc->wait_observed = true;
    proc->parent = NULL;

    spinlock_release_irqrestore(&process_lock, flags);
    return 0;
}

/*
 * Reap zombie processes whose kernel stacks are no longer active
 */
void process_reap_zombies(void) {
    uint64_t flags;
    spinlock_acquire_irqsave(&process_lock, &flags);

    struct process *current = current_process;

    for (int i = 1; i < MAX_PROCESSES; i++) {
        struct process *proc = &process_table[i];

        if (proc->state != PROCESS_ZOMBIE || proc == current) {
            continue;
        }

        if (proc->parent && !proc->wait_observed) {
            continue;
        }

        /* Free kernel stack */
        if (proc->kernel_stack_base) {
            extern uint64_t hhdm_offset;
            void *phys = (void *)(proc->kernel_stack_base - hhdm_offset);
            size_t pages = (KERNEL_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
            pmm_free_pages(phys, pages);
        }

        /* Mark slot as unused */
        for (int fd = 3; fd < PROCESS_MAX_FILES; fd++) {
            if (proc->files[fd].used && proc->files[fd].node) {
                vfs_close(proc->files[fd].node);
            }
        }

        if (proc->page_table && proc->page_table != vmm_get_kernel_pml4()) {
            vmm_destroy_address_space(proc->page_table);
        }

        memset(proc, 0, sizeof(*proc));
        proc->state = PROCESS_UNUSED;
    }

    spinlock_release_irqrestore(&process_lock, flags);
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

int process_fd_open(struct process *proc, struct vfs_node *node) {
    if (!proc || !node) return -1;

    for (int fd = 3; fd < PROCESS_MAX_FILES; fd++) {
        if (!proc->files[fd].used) {
            proc->files[fd].node = node;
            proc->files[fd].offset = 0;
            proc->files[fd].used = true;
            return fd;
        }
    }

    return -1;
}

int process_fd_read(struct process *proc, int fd, uint8_t *buffer, size_t size) {
    if (!proc || !buffer || size == 0) return -1;
    if (fd < 3 || fd >= PROCESS_MAX_FILES) return -1;
    if (!proc->files[fd].used || !proc->files[fd].node) return -1;

    int bytes = vfs_read(proc->files[fd].node, proc->files[fd].offset, size, buffer);
    if (bytes > 0) {
        proc->files[fd].offset += (uint64_t)bytes;
    }

    return bytes;
}

int process_fd_close(struct process *proc, int fd) {
    if (!proc) return -1;
    if (fd < 3 || fd >= PROCESS_MAX_FILES) return -1;
    if (!proc->files[fd].used) return -1;

    if (proc->files[fd].node) {
        vfs_close(proc->files[fd].node);
    }

    proc->files[fd].node = NULL;
    proc->files[fd].offset = 0;
    proc->files[fd].used = false;
    return 0;
}
