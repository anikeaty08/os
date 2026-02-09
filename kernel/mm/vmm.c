/*
 * AstraOS - Virtual Memory Manager Implementation
 * 4-level paging for x86_64
 */

#include "vmm.h"
#include "pmm.h"
#include "../lib/string.h"
#include "../arch/x86_64/cpu.h"
#include "../sync/spinlock.h"

/*
 * HHDM offset for physical to virtual conversion
 */
static uint64_t hhdm_offset = 0;

/*
 * Kernel PML4 (set by bootloader, we use it directly)
 */
static pagetable_t kernel_pml4 = NULL;

/*
 * VMM lock
 */
static spinlock_t vmm_lock = SPINLOCK_INIT;

/*
 * Page table index extraction macros
 */
#define PML4_INDEX(addr)    (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)    (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)      (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)      (((addr) >> 12) & 0x1FF)

/*
 * Physical to virtual address conversion
 */
static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

/*
 * Virtual to physical address conversion (for HHDM region)
 */
static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - hhdm_offset;
}

/*
 * Get or create next level page table
 */
static uint64_t *get_or_create_table(uint64_t *table, size_t index, uint64_t flags) {
    if (!(table[index] & PTE_PRESENT)) {
        /* Allocate new page table */
        void *new_table = pmm_alloc_page();
        if (!new_table) return NULL;

        /* Clear the new table */
        memset(phys_to_virt((uint64_t)new_table), 0, PAGE_SIZE);

        /* Set entry with requested flags */
        table[index] = (uint64_t)new_table | flags | PTE_PRESENT;
    }

    /* Return virtual address of next table */
    return phys_to_virt(table[index] & ~0xFFFULL);
}

/*
 * Get next level table (read-only, returns NULL if not present)
 */
static uint64_t *get_table(uint64_t *table, size_t index) {
    if (!(table[index] & PTE_PRESENT)) {
        return NULL;
    }
    return phys_to_virt(table[index] & ~0xFFFULL);
}

/*
 * Initialize VMM
 */
void vmm_init(uint64_t hhdm) {
    hhdm_offset = hhdm;

    /* Get current PML4 from CR3 */
    uint64_t cr3 = cpu_read_cr3();
    kernel_pml4 = phys_to_virt(cr3 & ~0xFFFULL);
}

/*
 * Get kernel PML4
 */
pagetable_t vmm_get_kernel_pml4(void) {
    return kernel_pml4;
}

/*
 * Create new address space
 */
pagetable_t vmm_create_address_space(void) {
    /* Allocate new PML4 */
    void *pml4_phys = pmm_alloc_page();
    if (!pml4_phys) return NULL;

    pagetable_t pml4 = phys_to_virt((uint64_t)pml4_phys);

    /* Clear it */
    memset(pml4, 0, PAGE_SIZE);

    /* Copy kernel mappings (upper half) */
    for (int i = 256; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }

    return pml4;
}

/*
 * Destroy address space
 */
void vmm_destroy_address_space(pagetable_t pml4) {
    if (!pml4 || pml4 == kernel_pml4) return;

    /* Only free user-space page tables (lower half) */
    for (int i = 0; i < 256; i++) {
        if (pml4[i] & PTE_PRESENT) {
            uint64_t *pdpt = get_table(pml4, i);
            if (pdpt) {
                for (int j = 0; j < 512; j++) {
                    if (pdpt[j] & PTE_PRESENT) {
                        uint64_t *pd = get_table(pdpt, j);
                        if (pd) {
                            for (int k = 0; k < 512; k++) {
                                if (pd[k] & PTE_PRESENT && !(pd[k] & PTE_HUGE)) {
                                    uint64_t *pt = get_table(pd, k);
                                    if (pt) {
                                        pmm_free_page((void *)(virt_to_phys(pt)));
                                    }
                                }
                            }
                            pmm_free_page((void *)(virt_to_phys(pd)));
                        }
                    }
                }
                pmm_free_page((void *)(virt_to_phys(pdpt)));
            }
        }
    }

    /* Free PML4 itself */
    pmm_free_page((void *)(virt_to_phys(pml4)));
}

/*
 * Map virtual to physical
 */
bool vmm_map_page(pagetable_t pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t irqflags;
    spinlock_acquire_irqsave(&vmm_lock, &irqflags);

    /* Use kernel PML4 if none specified */
    if (!pml4) pml4 = kernel_pml4;

    /* Walk page tables, creating as needed */
    uint64_t *pdpt = get_or_create_table(pml4, PML4_INDEX(virt), PTE_WRITABLE | PTE_USER);
    if (!pdpt) {
        spinlock_release_irqrestore(&vmm_lock, irqflags);
        return false;
    }

    uint64_t *pd = get_or_create_table(pdpt, PDPT_INDEX(virt), PTE_WRITABLE | PTE_USER);
    if (!pd) {
        spinlock_release_irqrestore(&vmm_lock, irqflags);
        return false;
    }

    uint64_t *pt = get_or_create_table(pd, PD_INDEX(virt), PTE_WRITABLE | PTE_USER);
    if (!pt) {
        spinlock_release_irqrestore(&vmm_lock, irqflags);
        return false;
    }

    /* Set the page table entry */
    pt[PT_INDEX(virt)] = (phys & ~0xFFFULL) | flags | PTE_PRESENT;

    /* Invalidate TLB */
    cpu_invlpg(virt);

    spinlock_release_irqrestore(&vmm_lock, irqflags);
    return true;
}

/*
 * Unmap virtual address
 */
void vmm_unmap_page(pagetable_t pml4, uint64_t virt) {
    uint64_t irqflags;
    spinlock_acquire_irqsave(&vmm_lock, &irqflags);

    if (!pml4) pml4 = kernel_pml4;

    uint64_t *pdpt = get_table(pml4, PML4_INDEX(virt));
    if (!pdpt) {
        spinlock_release_irqrestore(&vmm_lock, irqflags);
        return;
    }

    uint64_t *pd = get_table(pdpt, PDPT_INDEX(virt));
    if (!pd) {
        spinlock_release_irqrestore(&vmm_lock, irqflags);
        return;
    }

    uint64_t *pt = get_table(pd, PD_INDEX(virt));
    if (!pt) {
        spinlock_release_irqrestore(&vmm_lock, irqflags);
        return;
    }

    /* Clear the entry */
    pt[PT_INDEX(virt)] = 0;

    /* Invalidate TLB */
    cpu_invlpg(virt);

    spinlock_release_irqrestore(&vmm_lock, irqflags);
}

/*
 * Virtual to physical translation
 */
uint64_t vmm_virt_to_phys(pagetable_t pml4, uint64_t virt) {
    if (!pml4) pml4 = kernel_pml4;

    uint64_t *pdpt = get_table(pml4, PML4_INDEX(virt));
    if (!pdpt) return 0;

    uint64_t *pd = get_table(pdpt, PDPT_INDEX(virt));
    if (!pd) return 0;

    uint64_t *pt = get_table(pd, PD_INDEX(virt));
    if (!pt) return 0;

    if (!(pt[PT_INDEX(virt)] & PTE_PRESENT)) return 0;

    return (pt[PT_INDEX(virt)] & ~0xFFFULL) | (virt & 0xFFF);
}

/*
 * Switch address space
 */
void vmm_switch_address_space(pagetable_t pml4) {
    uint64_t phys = virt_to_phys(pml4);
    cpu_write_cr3(phys);
}

/*
 * Invalidate single TLB entry
 */
void vmm_invalidate_page(uint64_t virt) {
    cpu_invlpg(virt);
}
