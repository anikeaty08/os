/*
 * AstraOS - Global Descriptor Table Header
 * x86_64 GDT and TSS structures
 */

#ifndef _ASTRA_ARCH_GDT_H
#define _ASTRA_ARCH_GDT_H

#include <stdint.h>

/*
 * GDT Segment Selectors
 * These are offsets into the GDT (index * 8)
 */
#define GDT_NULL_SELECTOR        0x00
#define GDT_KERNEL_CODE_SELECTOR 0x08
#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_USER_CODE_SELECTOR   0x18
#define GDT_USER_DATA_SELECTOR   0x20
#define GDT_TSS_SELECTOR         0x28

/*
 * GDT Access Byte Flags
 */
#define GDT_ACCESS_PRESENT       (1 << 7)   /* Segment is present */
#define GDT_ACCESS_RING0         (0 << 5)   /* Ring 0 (kernel) */
#define GDT_ACCESS_RING3         (3 << 5)   /* Ring 3 (user) */
#define GDT_ACCESS_SYSTEM        (0 << 4)   /* System segment (TSS, LDT) */
#define GDT_ACCESS_CODE_DATA     (1 << 4)   /* Code or data segment */
#define GDT_ACCESS_EXECUTABLE    (1 << 3)   /* Executable (code segment) */
#define GDT_ACCESS_DC            (1 << 2)   /* Direction/Conforming */
#define GDT_ACCESS_RW            (1 << 1)   /* Readable (code) / Writable (data) */
#define GDT_ACCESS_ACCESSED      (1 << 0)   /* Accessed flag */

/*
 * GDT Flags (upper nibble of granularity byte)
 */
#define GDT_FLAG_GRANULARITY     (1 << 7)   /* 4KB page granularity */
#define GDT_FLAG_32BIT           (1 << 6)   /* 32-bit segment */
#define GDT_FLAG_64BIT           (1 << 5)   /* 64-bit code segment */

/*
 * TSS Type
 */
#define TSS_TYPE_AVAILABLE       0x9        /* 64-bit TSS (Available) */

/*
 * Standard GDT Entry (8 bytes)
 */
struct gdt_entry {
    uint16_t limit_low;      /* Limit bits 0-15 */
    uint16_t base_low;       /* Base bits 0-15 */
    uint8_t  base_middle;    /* Base bits 16-23 */
    uint8_t  access;         /* Access byte */
    uint8_t  granularity;    /* Flags + Limit bits 16-19 */
    uint8_t  base_high;      /* Base bits 24-31 */
} __attribute__((packed));

/*
 * TSS Entry in GDT (16 bytes for 64-bit)
 * TSS descriptor spans two GDT entries
 */
struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;     /* Upper 32 bits of base (64-bit) */
    uint32_t reserved;
} __attribute__((packed));

/*
 * GDT Pointer (for LGDT instruction)
 */
struct gdt_pointer {
    uint16_t limit;          /* Size of GDT - 1 */
    uint64_t base;           /* Linear address of GDT */
} __attribute__((packed));

/*
 * Task State Segment (64-bit)
 */
struct tss {
    uint32_t reserved0;
    uint64_t rsp0;           /* Stack pointer for ring 0 */
    uint64_t rsp1;           /* Stack pointer for ring 1 */
    uint64_t rsp2;           /* Stack pointer for ring 2 */
    uint64_t reserved1;
    uint64_t ist1;           /* Interrupt Stack Table 1 */
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;    /* I/O Permission Bitmap offset */
} __attribute__((packed));

/*
 * Initialize the GDT
 */
void gdt_init(void);

/*
 * Set the kernel stack pointer in TSS
 * Called during context switch to set the stack for ring 0
 */
void tss_set_rsp0(uint64_t rsp0);

/*
 * Get the current TSS
 */
struct tss *tss_get(void);

#endif /* _ASTRA_ARCH_GDT_H */
