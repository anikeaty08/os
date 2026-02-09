;
; AstraOS - Context Switch Implementation
; Saves and restores CPU context for process switching
;

section .text
bits 64

;------------------------------------------------------------------------------
; Context Switch
; void context_switch(struct cpu_context *old, struct cpu_context *new)
;
; RDI = pointer to old process context (NULL if first switch)
; RSI = pointer to new process context
;
; struct cpu_context layout (must match process.h):
;   offset 0:  r15
;   offset 8:  r14
;   offset 16: r13
;   offset 24: r12
;   offset 32: rbp
;   offset 40: rbx
;   offset 48: rip (return address)
;------------------------------------------------------------------------------
global context_switch
context_switch:
    ; Check if old context is NULL (first switch)
    test rdi, rdi
    jz .load_new

    ; Save callee-saved registers to old context
    mov [rdi + 0],  r15
    mov [rdi + 8],  r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], rbp
    mov [rdi + 40], rbx

    ; Save return address (on stack from call instruction)
    mov rax, [rsp]
    mov [rdi + 48], rax

    ; Save stack pointer
    ; We save RSP in a way that when we restore, the ret will work
    ; The return address is already saved, so we adjust
    lea rax, [rsp + 8]      ; Skip return address on stack
    mov [rdi + 56], rax     ; Save adjusted RSP (optional, not in struct)

.load_new:
    ; Load callee-saved registers from new context
    mov r15, [rsi + 0]
    mov r14, [rsi + 8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov rbp, [rsi + 32]
    mov rbx, [rsi + 40]

    ; Jump to new process
    ; Push return address and use ret to transfer control
    mov rax, [rsi + 48]     ; Get new RIP
    push rax
    ret                      ; "Return" to new process

;------------------------------------------------------------------------------
; Alternative simpler context switch using stack-based approach
; This version stores RSP instead of individual registers
;------------------------------------------------------------------------------

; struct simple_context {
;     uint64_t rsp;  // Stack pointer (everything else is on stack)
; };

; void simple_context_switch(uint64_t *old_rsp, uint64_t new_rsp)
global simple_context_switch
simple_context_switch:
    ; Save callee-saved registers on current stack
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    pushfq              ; Save flags

    ; Save current stack pointer
    test rdi, rdi
    jz .skip_save
    mov [rdi], rsp

.skip_save:
    ; Load new stack pointer
    mov rsp, rsi

    ; Restore callee-saved registers from new stack
    popfq               ; Restore flags
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Return to new context (return address is on new stack)
    ret
