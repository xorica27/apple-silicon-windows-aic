ENTRY(_start)

SECTIONS
{
    /* m1n1/DRAM load address; adjust if your m1n1 payload loader uses another. */
    . = 0x80000000;

    .text : ALIGN(16) {
        *(.text.boot)
        *(.text)
        *(.text.*)
    }

    .vectors : ALIGN(2048) {
        *(.vectors)
    }

    .rodata : ALIGN(16) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(16) {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . += 0x10000;
    __stack_top = .;
}
