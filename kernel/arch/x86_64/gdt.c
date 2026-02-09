/*
 * AstraOS - Global Descriptor Table Implementation
 * Sets up segmentation for x86_64 long mode
 */

#include "gdt.h"
#include "../../lib/string.h"

/*
 * GDT with 5 standard entries + 1 TSS entry (which takes 2 slots)
 * Entry 0: Null descriptor
 * Entry 1: Kernel Code (64-bit)
 * Entry 2: Kernel Data
 * Entry 3: User Code (64-bit)
 * Entry 4: User Data
 * Entry 5-6: TSS (16 bytes)
 */
static struct gdt_entry gdt[7];
static struct gdt_pointer gdtr;
static struct tss tss;

/* External assembly function to load GDT */
extern void gdt_load(struct gdt_pointer *gdtr, uint16_t code_sel, uint16_t data_sel);
extern void tss_load(uint16_t tss_sel);

/*
 * Set a standard GDT entry
 */
static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags) {
    gdt[index].base_low    = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high   = (base >> 24) & 0xFF;

    gdt[index].limit_low   = limit & 0xFFFF;
    gdt[index].granularity = ((limit >> 16) & 0x0F) | (flags & 0xF0);

    gdt[index].access      = access;
}

/*
 * Set the TSS entry in GDT
 */
static void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
    struct gdt_tss_entry *tss_entry = (struct gdt_tss_entry *)&gdt[index];

    tss_entry->limit_low   = limit & 0xFFFF;
    tss_entry->base_low    = base & 0xFFFF;
    tss_entry->base_middle = (base >> 16) & 0xFF;
    tss_entry->access      = GDT_ACCESS_PRESENT | TSS_TYPE_AVAILABLE;
    tss_entry->granularity = (limit >> 16) & 0x0F;
    tss_entry->base_high   = (base >> 24) & 0xFF;
    tss_entry->base_upper  = (base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved    = 0;
}

/*
 * Initialize GDT
 */
void gdt_init(void) {
    /* Clear GDT and TSS */
    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    /* Entry 0: Null descriptor (required) */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Entry 1: Kernel Code Segment (64-bit) */
    gdt_set_entry(1, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT |
        GDT_ACCESS_RING0 |
        GDT_ACCESS_CODE_DATA |
        GDT_ACCESS_EXECUTABLE |
        GDT_ACCESS_RW,
        GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY
    );

    /* Entry 2: Kernel Data Segment */
    gdt_set_entry(2, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT |
        GDT_ACCESS_RING0 |
        GDT_ACCESS_CODE_DATA |
        GDT_ACCESS_RW,
        GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY
    );

    /* Entry 3: User Code Segment (64-bit) */
    gdt_set_entry(3, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT |
        GDT_ACCESS_RING3 |
        GDT_ACCESS_CODE_DATA |
        GDT_ACCESS_EXECUTABLE |
        GDT_ACCESS_RW,
        GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY
    );

    /* Entry 4: User Data Segment */
    gdt_set_entry(4, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT |
        GDT_ACCESS_RING3 |
        GDT_ACCESS_CODE_DATA |
        GDT_ACCESS_RW,
        GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY
    );

    /* Initialize TSS */
    tss.iopb_offset = sizeof(tss);  /* No I/O permission bitmap */

    /* Entry 5-6: TSS (spans 2 entries in 64-bit mode) */
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss) - 1);

    /* Set up GDT pointer */
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    /* Load GDT and reload segment registers */
    gdt_load(&gdtr, GDT_KERNEL_CODE_SELECTOR, GDT_KERNEL_DATA_SELECTOR);

    /* Load TSS */
    tss_load(GDT_TSS_SELECTOR);
}

/*
 * Set kernel stack pointer in TSS
 * This is called during context switches to set the stack
 * that will be used when entering ring 0 from ring 3
 */
void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

/*
 * Get current TSS pointer
 */
struct tss *tss_get(void) {
    return &tss;
}
