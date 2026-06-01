;
; AstraOS - Ring 3 transition helper
;

section .text
bits 64

%define GDT_USER_CODE 0x18
%define GDT_USER_DATA 0x20
%define USER_RPL 0x03

; void user_enter(uint64_t entry, uint64_t user_stack)
; RDI = user RIP, RSI = user RSP
global user_enter
user_enter:
    mov ax, GDT_USER_DATA | USER_RPL
    mov ds, ax
    mov es, ax

    push qword (GDT_USER_DATA | USER_RPL)
    push rsi
    pushfq
    or qword [rsp], 0x200
    push qword (GDT_USER_CODE | USER_RPL)
    push rdi
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits
