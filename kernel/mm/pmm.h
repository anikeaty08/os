/*
 * AstraOS - Physical Memory Manager Header
 * Bitmap-based page frame allocator
 */

#ifndef _ASTRA_MM_PMM_H
#define _ASTRA_MM_PMM_H

#include <stdint.h>
#include <stddef.h>
#include "../limine.h"

/*
 * Page size (4 KB)
 */
#define PAGE_SIZE       4096
#define PAGE_SHIFT      12

/*
 * Align address up to page boundary
 */
#define PAGE_ALIGN_UP(addr)   (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/*
 * Align address down to page boundary
 */
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

/*
 * Initialize PMM with memory map from bootloader
 */
void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);

/*
 * Allocate a single physical page
 * Returns physical address, or 0 on failure
 */
void *pmm_alloc_page(void);

/*
 * Allocate multiple contiguous physical pages
 * Returns physical address, or NULL on failure
 */
void *pmm_alloc_pages(size_t count);

/*
 * Free a single physical page
 */
void pmm_free_page(void *page);

/*
 * Free multiple contiguous physical pages
 */
void pmm_free_pages(void *page, size_t count);

/*
 * Get total physical memory (bytes)
 */
uint64_t pmm_get_total_memory(void);

/*
 * Get free physical memory (bytes)
 */
uint64_t pmm_get_free_memory(void);

/*
 * Get used physical memory (bytes)
 */
uint64_t pmm_get_used_memory(void);

#endif /* _ASTRA_MM_PMM_H */
