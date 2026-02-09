/* linker.ld - Kernel linker script for x86_64 higher-half kernel */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

/* Higher-half kernel base address */
KERNEL_VBASE = 0xffffffff80000000;

PHDRS {
    limine_requests PT_LOAD;
    text            PT_LOAD FLAGS(5);   /* Read + Execute */
    rodata          PT_LOAD FLAGS(4);   /* Read only */
    data            PT_LOAD FLAGS(6);   /* Read + Write */
}

SECTIONS {
    . = KERNEL_VBASE;

    /* Limine bootloader requests - must be at start */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(4K);

    /* Code section */
    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(4K);

    /* Read-only data */
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(4K);

    /* Read-write data */
    .data : {
        *(.data .data.*)
    } :data

    /* Uninitialized data (BSS) */
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}
