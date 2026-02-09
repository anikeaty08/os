/*
 * AstraOS - Virtual Memory Manager Header
 * 4-level paging for x86_64
 */

#ifndef _ASTRA_MM_VMM_H
#define _ASTRA_MM_VMM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Page table entry flags
 */
#define PTE_PRESENT     (1ULL << 0)     /* Page is present */
#define PTE_WRITABLE    (1ULL << 1)     /* Page is writable */
#define PTE_USER        (1ULL << 2)     /* User-mode accessible */
#define PTE_WRITETHROUGH (1ULL << 3)    /* Write-through caching */
#define PTE_NOCACHE     (1ULL << 4)     /* Disable caching */
#define PTE_ACCESSED    (1ULL << 5)     /* Page was accessed */
#define PTE_DIRTY       (1ULL << 6)     /* Page was written */
#define PTE_HUGE        (1ULL << 7)     /* Huge page (2MB/1GB) */
#define PTE_GLOBAL      (1ULL << 8)     /* Global page */
#define PTE_NX          (1ULL << 63)    /* No execute */

/*
 * Page table structure
 */
#define PT_ENTRIES      512

/*
 * Virtual address space regions
 */
#define KERNEL_VBASE    0xFFFFFFFF80000000ULL   /* Kernel virtual base */
#define HEAP_VBASE      0xFFFF800100000000ULL   /* Kernel heap start */

/*
 * Page table type
 */
typedef uint64_t *pagetable_t;

/*
 * Initialize VMM
 */
void vmm_init(uint64_t hhdm_offset);

/*
 * Get kernel page table
 */
pagetable_t vmm_get_kernel_pml4(void);

/*
 * Create new address space (PML4)
 */
pagetable_t vmm_create_address_space(void);

/*
 * Destroy address space
 */
void vmm_destroy_address_space(pagetable_t pml4);

/*
 * Map virtual address to physical
 * Returns true on success
 */
bool vmm_map_page(pagetable_t pml4, uint64_t virt, uint64_t phys, uint64_t flags);

/*
 * Unmap virtual address
 */
void vmm_unmap_page(pagetable_t pml4, uint64_t virt);

/*
 * Get physical address for virtual address
 * Returns 0 if not mapped
 */
uint64_t vmm_virt_to_phys(pagetable_t pml4, uint64_t virt);

/*
 * Switch to address space
 */
void vmm_switch_address_space(pagetable_t pml4);

/*
 * Invalidate TLB entry
 */
void vmm_invalidate_page(uint64_t virt);

#endif /* _ASTRA_MM_VMM_H */
