;
; AstraOS - GDT Assembly Routines
; Load GDT and reload segment registers
;

section .text
bits 64

; void gdt_load(struct gdt_pointer *gdtr, uint16_t code_sel, uint16_t data_sel)
; RDI = pointer to GDT descriptor
; RSI = kernel code segment selector
; RDX = kernel data segment selector
global gdt_load
gdt_load:
    ; Load the GDT
    lgdt [rdi]

    ; Reload data segment registers
    mov ax, dx          ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS using a far return
    ; Push the new CS and the return address, then use retfq
    pop rax             ; Get return address
    push rsi            ; Push new CS (kernel code selector)
    push rax            ; Push return address
    retfq               ; Far return to reload CS

; void tss_load(uint16_t tss_selector)
; RDI = TSS segment selector
global tss_load
tss_load:
    mov ax, di
    ltr ax              ; Load Task Register
    ret
