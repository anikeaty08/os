;
; AstraOS - int 0x80 syscall entry
;

section .text
bits 64

extern syscall_handler

global syscall_stub
syscall_stub:
    ; Match struct interrupt_frame layout used by C handlers.
    push qword 0x0
    push qword 0x80

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

    mov rdi, rsp
    mov rbp, rsp
    and rsp, ~0xF
    call syscall_handler
    mov rsp, rbp

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

    add rsp, 16
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits
