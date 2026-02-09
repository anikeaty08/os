/*
 * AstraOS - Interrupt Service Routines Implementation
 * Exception and interrupt handling
 */

#include "isr.h"
#include "idt.h"
#include "irq.h"
#include "cpu.h"
#include "../../panic.h"
#include "../../drivers/serial.h"
#include "../../lib/string.h"

/*
 * Exception names for debugging
 */
const char *exception_names[32] = {
    "Division Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security",
    "Reserved"
};

/*
 * Simple hex print for debug
 */
static void print_hex(uint64_t value) {
    const char *hex = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buf[2 + (15 - i)] = hex[(value >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
    serial_puts(buf);
}

/*
 * Handle CPU exception
 */
static void handle_exception(struct interrupt_frame *frame) {
    serial_puts("\n!!! CPU EXCEPTION !!!\n");
    serial_puts("Exception: ");
    serial_puts(exception_names[frame->int_no]);
    serial_puts(" (#");

    /* Print exception number */
    char num[4];
    num[0] = '0' + (frame->int_no / 10);
    num[1] = '0' + (frame->int_no % 10);
    num[2] = ')';
    num[3] = '\0';
    serial_puts(num);
    serial_puts("\n");

    /* Print error code if applicable */
    if (frame->int_no == EXCEPTION_DF ||
        frame->int_no == EXCEPTION_TS ||
        frame->int_no == EXCEPTION_NP ||
        frame->int_no == EXCEPTION_SS ||
        frame->int_no == EXCEPTION_GP ||
        frame->int_no == EXCEPTION_PF ||
        frame->int_no == EXCEPTION_AC ||
        frame->int_no == EXCEPTION_CP ||
        frame->int_no == EXCEPTION_VC ||
        frame->int_no == EXCEPTION_SX) {
        serial_puts("Error Code: ");
        print_hex(frame->error_code);
        serial_puts("\n");
    }

    /* For page fault, print CR2 (faulting address) */
    if (frame->int_no == EXCEPTION_PF) {
        serial_puts("Faulting Address (CR2): ");
        print_hex(cpu_read_cr2());
        serial_puts("\n");

        /* Decode page fault error code */
        serial_puts("Cause: ");
        if (frame->error_code & 0x01) {
            serial_puts("Protection violation, ");
        } else {
            serial_puts("Non-present page, ");
        }
        if (frame->error_code & 0x02) {
            serial_puts("Write, ");
        } else {
            serial_puts("Read, ");
        }
        if (frame->error_code & 0x04) {
            serial_puts("User mode");
        } else {
            serial_puts("Kernel mode");
        }
        serial_puts("\n");
    }

    /* Print register state */
    serial_puts("\nRegisters:\n");
    serial_puts("  RIP: "); print_hex(frame->rip); serial_puts("\n");
    serial_puts("  RSP: "); print_hex(frame->rsp); serial_puts("\n");
    serial_puts("  RBP: "); print_hex(frame->rbp); serial_puts("\n");
    serial_puts("  RAX: "); print_hex(frame->rax); serial_puts("\n");
    serial_puts("  RBX: "); print_hex(frame->rbx); serial_puts("\n");
    serial_puts("  RCX: "); print_hex(frame->rcx); serial_puts("\n");
    serial_puts("  RDX: "); print_hex(frame->rdx); serial_puts("\n");
    serial_puts("  RSI: "); print_hex(frame->rsi); serial_puts("\n");
    serial_puts("  RDI: "); print_hex(frame->rdi); serial_puts("\n");
    serial_puts("  CS:  "); print_hex(frame->cs); serial_puts("\n");
    serial_puts("  SS:  "); print_hex(frame->ss); serial_puts("\n");
    serial_puts("  RFLAGS: "); print_hex(frame->rflags); serial_puts("\n");

    /* Halt - unrecoverable */
    panic("Unhandled CPU exception");
}

/*
 * Main ISR handler
 * Called from assembly stub with pointer to interrupt frame
 */
void isr_handler(struct interrupt_frame *frame) {
    uint64_t int_no = frame->int_no;

    if (int_no < 32) {
        /* CPU Exception */
        handle_exception(frame);
    } else if (int_no < 48) {
        /* Hardware IRQ (32-47) */
        uint8_t irq = int_no - 32;

        /* Dispatch to registered handler */
        irq_dispatch(irq);

        /* Send EOI */
        irq_eoi(irq);
    } else {
        /* Other interrupt - just acknowledge */
        serial_puts("Unhandled interrupt: ");
        print_hex(int_no);
        serial_puts("\n");
    }
}

/*
 * Install all ISR stubs into the IDT
 */
void isr_install(void) {
    /* CPU Exceptions (0-31) */
    for (int i = 0; i < 32; i++) {
        idt_set_entry(i, isr_stub_table[i],
            IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_TYPE_INTERRUPT,
            0
        );
    }

    /* Hardware IRQs (32-47) */
    for (int i = 32; i < 48; i++) {
        idt_set_entry(i, isr_stub_table[i],
            IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_TYPE_INTERRUPT,
            0
        );
    }
}
