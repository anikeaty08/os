;
; AstraOS - IDT and ISR Assembly Routines
; Interrupt handling stubs for x86_64
;

section .text
bits 64

; External C handler
extern isr_handler

;------------------------------------------------------------------------------
; Load IDT
; void idt_load(struct idt_pointer *idtr)
;------------------------------------------------------------------------------
global idt_load
idt_load:
    lidt [rdi]          ; Load IDT from pointer in RDI
    ret

;------------------------------------------------------------------------------
; ISR Stub Macros
; We need different macros for exceptions with/without error codes
;------------------------------------------------------------------------------

; Macro for ISR without error code
; CPU doesn't push error code, so we push 0
%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push qword 0        ; Dummy error code
    push qword %1       ; Interrupt number
    jmp isr_common
%endmacro

; Macro for ISR with error code
; CPU already pushed error code
%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    push qword %1       ; Interrupt number (error code already on stack)
    jmp isr_common
%endmacro

;------------------------------------------------------------------------------
; CPU Exception Stubs (0-31)
;------------------------------------------------------------------------------
ISR_NOERR 0     ; #DE - Divide Error
ISR_NOERR 1     ; #DB - Debug
ISR_NOERR 2     ; NMI - Non-Maskable Interrupt
ISR_NOERR 3     ; #BP - Breakpoint
ISR_NOERR 4     ; #OF - Overflow
ISR_NOERR 5     ; #BR - Bound Range Exceeded
ISR_NOERR 6     ; #UD - Invalid Opcode
ISR_NOERR 7     ; #NM - Device Not Available
ISR_ERR   8     ; #DF - Double Fault (always 0)
ISR_NOERR 9     ; Coprocessor Segment Overrun (reserved)
ISR_ERR   10    ; #TS - Invalid TSS
ISR_ERR   11    ; #NP - Segment Not Present
ISR_ERR   12    ; #SS - Stack-Segment Fault
ISR_ERR   13    ; #GP - General Protection Fault
ISR_ERR   14    ; #PF - Page Fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; #MF - x87 FPU Error
ISR_ERR   17    ; #AC - Alignment Check
ISR_NOERR 18    ; #MC - Machine Check
ISR_NOERR 19    ; #XM - SIMD Floating-Point
ISR_NOERR 20    ; #VE - Virtualization
ISR_ERR   21    ; #CP - Control Protection
ISR_NOERR 22    ; Reserved
ISR_NOERR 23    ; Reserved
ISR_NOERR 24    ; Reserved
ISR_NOERR 25    ; Reserved
ISR_NOERR 26    ; Reserved
ISR_NOERR 27    ; Reserved
ISR_NOERR 28    ; #HV - Hypervisor Injection
ISR_ERR   29    ; #VC - VMM Communication
ISR_ERR   30    ; #SX - Security
ISR_NOERR 31    ; Reserved

;------------------------------------------------------------------------------
; Hardware IRQ Stubs (32-47)
; These are remapped from IRQ 0-15
;------------------------------------------------------------------------------
ISR_NOERR 32    ; IRQ 0 - Timer
ISR_NOERR 33    ; IRQ 1 - Keyboard
ISR_NOERR 34    ; IRQ 2 - Cascade
ISR_NOERR 35    ; IRQ 3 - COM2
ISR_NOERR 36    ; IRQ 4 - COM1
ISR_NOERR 37    ; IRQ 5 - LPT2 / Sound
ISR_NOERR 38    ; IRQ 6 - Floppy
ISR_NOERR 39    ; IRQ 7 - LPT1 (spurious)
ISR_NOERR 40    ; IRQ 8 - RTC
ISR_NOERR 41    ; IRQ 9 - ACPI
ISR_NOERR 42    ; IRQ 10 - Available
ISR_NOERR 43    ; IRQ 11 - Available
ISR_NOERR 44    ; IRQ 12 - Mouse
ISR_NOERR 45    ; IRQ 13 - FPU
ISR_NOERR 46    ; IRQ 14 - Primary ATA
ISR_NOERR 47    ; IRQ 15 - Secondary ATA (spurious)

;------------------------------------------------------------------------------
; Common ISR Handler
; Saves all registers, calls C handler, restores registers
;------------------------------------------------------------------------------
isr_common:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Save segment registers (for debugging)
    ; Note: In 64-bit mode, DS/ES/FS/GS are mostly unused
    ; but we want a complete context

    ; Pass pointer to interrupt frame as argument
    mov rdi, rsp

    ; Align stack to 16 bytes (ABI requirement)
    ; RSP is already 8-byte aligned after pushes
    ; We need to save old RSP and align
    mov rbp, rsp
    and rsp, ~0xF

    ; Call C handler
    call isr_handler

    ; Restore stack
    mov rsp, rbp

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove interrupt number and error code from stack
    add rsp, 16

    ; Return from interrupt
    iretq

;------------------------------------------------------------------------------
; ISR Stub Table
; Array of pointers to all ISR stubs
;------------------------------------------------------------------------------
section .data

global isr_stub_table
isr_stub_table:
    ; CPU Exceptions (0-31)
    dq isr_stub_0
    dq isr_stub_1
    dq isr_stub_2
    dq isr_stub_3
    dq isr_stub_4
    dq isr_stub_5
    dq isr_stub_6
    dq isr_stub_7
    dq isr_stub_8
    dq isr_stub_9
    dq isr_stub_10
    dq isr_stub_11
    dq isr_stub_12
    dq isr_stub_13
    dq isr_stub_14
    dq isr_stub_15
    dq isr_stub_16
    dq isr_stub_17
    dq isr_stub_18
    dq isr_stub_19
    dq isr_stub_20
    dq isr_stub_21
    dq isr_stub_22
    dq isr_stub_23
    dq isr_stub_24
    dq isr_stub_25
    dq isr_stub_26
    dq isr_stub_27
    dq isr_stub_28
    dq isr_stub_29
    dq isr_stub_30
    dq isr_stub_31
    ; Hardware IRQs (32-47)
    dq isr_stub_32
    dq isr_stub_33
    dq isr_stub_34
    dq isr_stub_35
    dq isr_stub_36
    dq isr_stub_37
    dq isr_stub_38
    dq isr_stub_39
    dq isr_stub_40
    dq isr_stub_41
    dq isr_stub_42
    dq isr_stub_43
    dq isr_stub_44
    dq isr_stub_45
    dq isr_stub_46
    dq isr_stub_47
