ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

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
    . += 0x40000;
    __stack_top = .;
}
