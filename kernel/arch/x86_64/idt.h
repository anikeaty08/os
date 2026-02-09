/*
 * AstraOS - Interrupt Descriptor Table Header
 * x86_64 IDT structures and management
 */

#ifndef _ASTRA_ARCH_IDT_H
#define _ASTRA_ARCH_IDT_H

#include <stdint.h>

/*
 * Number of IDT entries (0-255)
 */
#define IDT_ENTRIES     256

/*
 * IDT Gate Types
 */
#define IDT_TYPE_INTERRUPT  0x0E    /* Interrupt gate (clears IF) */
#define IDT_TYPE_TRAP       0x0F    /* Trap gate (doesn't clear IF) */

/*
 * IDT Gate Flags
 */
#define IDT_FLAG_PRESENT    (1 << 7)
#define IDT_FLAG_DPL0       (0 << 5)    /* Ring 0 */
#define IDT_FLAG_DPL3       (3 << 5)    /* Ring 3 */

/*
 * CPU Exception Vectors
 */
#define EXCEPTION_DE        0   /* Divide Error */
#define EXCEPTION_DB        1   /* Debug */
#define EXCEPTION_NMI       2   /* Non-Maskable Interrupt */
#define EXCEPTION_BP        3   /* Breakpoint */
#define EXCEPTION_OF        4   /* Overflow */
#define EXCEPTION_BR        5   /* Bound Range Exceeded */
#define EXCEPTION_UD        6   /* Invalid Opcode */
#define EXCEPTION_NM        7   /* Device Not Available */
#define EXCEPTION_DF        8   /* Double Fault */
#define EXCEPTION_CSO       9   /* Coprocessor Segment Overrun */
#define EXCEPTION_TS        10  /* Invalid TSS */
#define EXCEPTION_NP        11  /* Segment Not Present */
#define EXCEPTION_SS        12  /* Stack-Segment Fault */
#define EXCEPTION_GP        13  /* General Protection Fault */
#define EXCEPTION_PF        14  /* Page Fault */
#define EXCEPTION_RESERVED  15  /* Reserved */
#define EXCEPTION_MF        16  /* x87 FPU Error */
#define EXCEPTION_AC        17  /* Alignment Check */
#define EXCEPTION_MC        18  /* Machine Check */
#define EXCEPTION_XM        19  /* SIMD Floating-Point */
#define EXCEPTION_VE        20  /* Virtualization */
#define EXCEPTION_CP        21  /* Control Protection */
/* 22-27 Reserved */
#define EXCEPTION_HV        28  /* Hypervisor Injection */
#define EXCEPTION_VC        29  /* VMM Communication */
#define EXCEPTION_SX        30  /* Security */
/* 31 Reserved */

/*
 * IDT Entry (Gate Descriptor) - 16 bytes for 64-bit
 */
struct idt_entry {
    uint16_t offset_low;     /* Offset bits 0-15 */
    uint16_t selector;       /* Code segment selector */
    uint8_t  ist;            /* Interrupt Stack Table (bits 0-2), 0 = legacy */
    uint8_t  type_attr;      /* Type and attributes */
    uint16_t offset_mid;     /* Offset bits 16-31 */
    uint32_t offset_high;    /* Offset bits 32-63 */
    uint32_t reserved;       /* Must be zero */
} __attribute__((packed));

/*
 * IDT Pointer (for LIDT instruction)
 */
struct idt_pointer {
    uint16_t limit;          /* Size of IDT - 1 */
    uint64_t base;           /* Linear address of IDT */
} __attribute__((packed));

/*
 * Interrupt Stack Frame
 * This is pushed onto the stack by the CPU and ISR stub
 */
struct interrupt_frame {
    /* Pushed by our ISR stub */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    /* Pushed by our ISR stub */
    uint64_t int_no;         /* Interrupt number */
    uint64_t error_code;     /* Error code (or 0) */

    /* Pushed by CPU */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

/*
 * Initialize the IDT
 */
void idt_init(void);

/*
 * Set an IDT entry
 */
void idt_set_entry(uint8_t vector, void *handler, uint8_t type_attr, uint8_t ist);

/*
 * Install a simple interrupt handler
 */
void idt_set_handler(uint8_t vector, void *handler);

#endif /* _ASTRA_ARCH_IDT_H */
