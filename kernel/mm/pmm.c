/*
 * AstraOS - Physical Memory Manager Implementation
 * Bitmap-based page frame allocator
 */

#include "pmm.h"
#include "../lib/string.h"
#include "../sync/spinlock.h"

/*
 * Bitmap for tracking page allocation
 * Each bit represents one 4KB page
 * 1 = allocated, 0 = free
 */
static uint8_t *bitmap = NULL;
static uint64_t bitmap_size = 0;      /* Size in bytes */
static uint64_t total_pages = 0;
static uint64_t used_pages = 0;
static uint64_t highest_page = 0;

/*
 * HHDM offset for converting physical to virtual
 */
static uint64_t hhdm = 0;

/*
 * Spinlock for thread safety
 */
static spinlock_t pmm_lock = SPINLOCK_INIT;

/*
 * Bitmap operations
 */
static inline void bitmap_set(uint64_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int bitmap_test(uint64_t page) {
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

/*
 * Convert physical address to virtual using HHDM
 */
static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm);
}

/*
 * Convert virtual address to physical
 */
static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - hhdm;
}

/*
 * Initialize PMM
 */
void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
    hhdm = hhdm_offset;

    /* Find highest usable address to determine bitmap size */
    uint64_t highest_addr = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        uint64_t top = entry->base + entry->length;
        if (top > highest_addr) {
            highest_addr = top;
        }
    }

    /* Calculate bitmap size (need to track all address space for safety) */
    highest_page = highest_addr / PAGE_SIZE;
    bitmap_size = (highest_page + 7) / 8;

    /* Count only usable memory for total_pages */
    total_pages = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_page = PAGE_ALIGN_UP(entry->base) / PAGE_SIZE;
            uint64_t end_page = PAGE_ALIGN_DOWN(entry->base + entry->length) / PAGE_SIZE;
            total_pages += (end_page - start_page);
        }
    }

    /* Find a usable region for the bitmap */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            bitmap = phys_to_virt(entry->base);
            break;
        }
    }

    if (!bitmap) {
        /* Can't proceed without bitmap - would need panic here */
        return;
    }

    /* Mark all pages as used initially */
    memset(bitmap, 0xFF, bitmap_size);
    used_pages = 0;  /* Start at 0, will be counted properly */

    /* Free usable memory regions */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_page = PAGE_ALIGN_UP(entry->base) / PAGE_SIZE;
            uint64_t end_page = PAGE_ALIGN_DOWN(entry->base + entry->length) / PAGE_SIZE;

            for (uint64_t page = start_page; page < end_page; page++) {
                bitmap_clear(page);  /* Clear the bit to mark as free */
            }
        }
    }

    /* Reserve the bitmap itself */
    uint64_t bitmap_phys = virt_to_phys(bitmap);
    uint64_t bitmap_pages = PAGE_ALIGN_UP(bitmap_size) / PAGE_SIZE;
    uint64_t bitmap_start = bitmap_phys / PAGE_SIZE;

    for (uint64_t i = 0; i < bitmap_pages; i++) {
        if (!bitmap_test(bitmap_start + i)) {
            bitmap_set(bitmap_start + i);
            used_pages++;
        }
    }

    /* Reserve first page (null pointer protection) */
    if (!bitmap_test(0)) {
        bitmap_set(0);
        used_pages++;
    }
}

/*
 * Allocate single page
 */
void *pmm_alloc_page(void) {
    uint64_t flags;
    spinlock_acquire_irqsave(&pmm_lock, &flags);

    /* Linear search for free page */
    for (uint64_t i = 1; i < highest_page; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            spinlock_release_irqrestore(&pmm_lock, flags);
            return (void *)(i * PAGE_SIZE);
        }
    }

    spinlock_release_irqrestore(&pmm_lock, flags);
    return NULL;  /* Out of memory */
}

/*
 * Allocate contiguous pages
 */
void *pmm_alloc_pages(size_t count) {
    if (count == 0) return NULL;
    if (count == 1) return pmm_alloc_page();

    uint64_t flags;
    spinlock_acquire_irqsave(&pmm_lock, &flags);

    /* Search for contiguous free pages */
    uint64_t consecutive = 0;
    uint64_t start_page = 0;

    for (uint64_t i = 1; i < highest_page; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) {
                start_page = i;
            }
            consecutive++;

            if (consecutive == count) {
                /* Found enough pages, mark them as used */
                for (uint64_t j = 0; j < count; j++) {
                    bitmap_set(start_page + j);
                }
                used_pages += count;
                spinlock_release_irqrestore(&pmm_lock, flags);
                return (void *)(start_page * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }

    spinlock_release_irqrestore(&pmm_lock, flags);
    return NULL;  /* Not enough contiguous memory */
}

/*
 * Free single page
 */
void pmm_free_page(void *page) {
    if (!page) return;

    uint64_t page_num = (uint64_t)page / PAGE_SIZE;
    if (page_num >= highest_page) return;

    uint64_t flags;
    spinlock_acquire_irqsave(&pmm_lock, &flags);

    if (bitmap_test(page_num)) {
        bitmap_clear(page_num);
        used_pages--;
    }

    spinlock_release_irqrestore(&pmm_lock, flags);
}

/*
 * Free multiple pages
 */
void pmm_free_pages(void *page, size_t count) {
    if (!page || count == 0) return;

    uint64_t start = (uint64_t)page / PAGE_SIZE;

    uint64_t flags;
    spinlock_acquire_irqsave(&pmm_lock, &flags);

    for (size_t i = 0; i < count; i++) {
        uint64_t page_num = start + i;
        if (page_num < highest_page && bitmap_test(page_num)) {
            bitmap_clear(page_num);
            used_pages--;
        }
    }

    spinlock_release_irqrestore(&pmm_lock, flags);
}

/*
 * Memory statistics
 */
uint64_t pmm_get_total_memory(void) {
    return total_pages * PAGE_SIZE;
}

uint64_t pmm_get_free_memory(void) {
    return (total_pages - used_pages) * PAGE_SIZE;
}

uint64_t pmm_get_used_memory(void) {
    return used_pages * PAGE_SIZE;
}
