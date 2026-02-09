/*
 * AstraOS - Kernel Heap Implementation
 * Simple first-fit block allocator
 */

#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../lib/string.h"
#include "../sync/spinlock.h"

/*
 * Heap configuration
 */
#define HEAP_START          0xFFFF800100000000ULL
#define HEAP_INITIAL_SIZE   (64 * PAGE_SIZE)    /* 256 KB initial */
#define HEAP_BLOCK_MAGIC    0xDEADBEEF
#define MIN_BLOCK_SIZE      32
#define ALIGNMENT           16

/*
 * Block header structure
 */
struct heap_block {
    uint32_t magic;             /* Magic number for validation */
    uint32_t size;              /* Size of data area (excluding header) */
    struct heap_block *next;    /* Next block in list */
    struct heap_block *prev;    /* Previous block in list */
    uint8_t free;               /* Is this block free? */
    uint8_t padding[7];         /* Padding for alignment */
};

/*
 * Heap state
 */
static struct heap_block *heap_start = NULL;
static struct heap_block *heap_end = NULL;
static uint64_t heap_top = HEAP_START;
static size_t total_allocated = 0;
static spinlock_t heap_lock = SPINLOCK_INIT;

/*
 * Expand heap by mapping more pages
 */
static int heap_expand(size_t min_size) {
    size_t pages_needed = (min_size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_needed < 4) pages_needed = 4;  /* Minimum expansion */

    for (size_t i = 0; i < pages_needed; i++) {
        void *page = pmm_alloc_page();
        if (!page) return -1;

        if (!vmm_map_page(NULL, heap_top, (uint64_t)page, PTE_WRITABLE)) {
            pmm_free_page(page);
            return -1;
        }

        heap_top += PAGE_SIZE;
    }

    return 0;
}

/*
 * Initialize heap
 */
void heap_init(void) {
    /* Map initial heap pages */
    for (size_t i = 0; i < HEAP_INITIAL_SIZE; i += PAGE_SIZE) {
        void *page = pmm_alloc_page();
        if (!page) return;

        vmm_map_page(NULL, HEAP_START + i, (uint64_t)page, PTE_WRITABLE);
    }

    heap_top = HEAP_START + HEAP_INITIAL_SIZE;

    /* Initialize first block spanning entire heap */
    heap_start = (struct heap_block *)HEAP_START;
    heap_start->magic = HEAP_BLOCK_MAGIC;
    heap_start->size = HEAP_INITIAL_SIZE - sizeof(struct heap_block);
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->free = 1;

    heap_end = heap_start;
}

/*
 * Allocate memory
 */
void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Align size */
    size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE;

    uint64_t flags;
    spinlock_acquire_irqsave(&heap_lock, &flags);

    /* First-fit search */
    struct heap_block *block = heap_start;
    while (block) {
        if (block->free && block->size >= size) {
            /* Check if we can split */
            if (block->size >= size + sizeof(struct heap_block) + MIN_BLOCK_SIZE) {
                /* Split block */
                struct heap_block *new_block = (struct heap_block *)
                    ((uint8_t *)block + sizeof(struct heap_block) + size);

                new_block->magic = HEAP_BLOCK_MAGIC;
                new_block->size = block->size - size - sizeof(struct heap_block);
                new_block->next = block->next;
                new_block->prev = block;
                new_block->free = 1;

                if (block->next) {
                    block->next->prev = new_block;
                }
                block->next = new_block;
                block->size = size;

                if (block == heap_end) {
                    heap_end = new_block;
                }
            }

            block->free = 0;
            total_allocated += block->size;

            spinlock_release_irqrestore(&heap_lock, flags);
            return (void *)((uint8_t *)block + sizeof(struct heap_block));
        }
        block = block->next;
    }

    /* No suitable block found, expand heap */
    size_t expand_size = size + sizeof(struct heap_block);
    if (heap_expand(expand_size) < 0) {
        spinlock_release_irqrestore(&heap_lock, flags);
        return NULL;
    }

    /* Create new block at end */
    struct heap_block *new_block = (struct heap_block *)
        ((uint8_t *)heap_end + sizeof(struct heap_block) + heap_end->size);

    /* Check if this is in the newly mapped area */
    if ((uint64_t)new_block >= heap_top) {
        spinlock_release_irqrestore(&heap_lock, flags);
        return NULL;
    }

    new_block->magic = HEAP_BLOCK_MAGIC;
    new_block->size = heap_top - (uint64_t)new_block - sizeof(struct heap_block);
    new_block->next = NULL;
    new_block->prev = heap_end;
    new_block->free = 1;

    heap_end->next = new_block;
    heap_end = new_block;

    /* Retry allocation */
    spinlock_release_irqrestore(&heap_lock, flags);
    return kmalloc(size);
}

/*
 * Allocate zeroed memory
 */
void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/*
 * Reallocate memory
 */
void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct heap_block *block = (struct heap_block *)
        ((uint8_t *)ptr - sizeof(struct heap_block));

    if (block->magic != HEAP_BLOCK_MAGIC) {
        return NULL;  /* Invalid pointer */
    }

    if (block->size >= new_size) {
        return ptr;  /* Already big enough */
    }

    /* Allocate new block and copy */
    void *new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}

/*
 * Free memory
 */
void kfree(void *ptr) {
    if (!ptr) return;

    struct heap_block *block = (struct heap_block *)
        ((uint8_t *)ptr - sizeof(struct heap_block));

    if (block->magic != HEAP_BLOCK_MAGIC) {
        return;  /* Invalid pointer */
    }

    uint64_t flags;
    spinlock_acquire_irqsave(&heap_lock, &flags);

    block->free = 1;
    total_allocated -= block->size;

    /* Coalesce with next block if free */
    if (block->next && block->next->free) {
        block->size += sizeof(struct heap_block) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
        if (heap_end == block->next) {
            heap_end = block;
        }
    }

    /* Coalesce with previous block if free */
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(struct heap_block) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        if (heap_end == block) {
            heap_end = block->prev;
        }
    }

    spinlock_release_irqrestore(&heap_lock, flags);
}

/*
 * Statistics
 */
size_t heap_get_used(void) {
    return total_allocated;
}

size_t heap_get_free(void) {
    size_t free = 0;
    struct heap_block *block = heap_start;
    while (block) {
        if (block->free) {
            free += block->size;
        }
        block = block->next;
    }
    return free;
}
