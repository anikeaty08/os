;
; AstraOS user-space C runtime entry
;

section .text
bits 64

extern main
global _start

_start:
    xor rbp, rbp
    call main
    mov rdi, rax
    mov rax, 0
    int 0x80

section .note.GNU-stack noalloc noexec nowrite progbits
