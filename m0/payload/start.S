.section .text.boot, "ax"
.global _start

_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbnz x0, park

    ldr x0, =__stack_top
    mov sp, x0

    ldr x0, =__bss_start
    ldr x1, =__bss_end
1:
    cmp x0, x1
    b.hs 2f
    str xzr, [x0], #8
    b 1b
2:
    ldr x0, =vectors
    msr vbar_el1, x0
    isb

    bl main

park:
    wfi
    b park

.macro vector_entry label
    .align 7
    b \label
.endm

.section .vectors, "ax"
.align 11
.global vectors

vectors:
    vector_entry default_handler
    vector_entry default_handler
    vector_entry default_handler
    vector_entry default_handler

    vector_entry default_handler
    vector_entry irq_handler
    vector_entry fiq_handler
    vector_entry default_handler

    vector_entry default_handler
    vector_entry default_handler
    vector_entry default_handler
    vector_entry default_handler

    vector_entry default_handler
    vector_entry default_handler
    vector_entry default_handler
    vector_entry default_handler

.section .text, "ax"

irq_handler:
    sub sp, sp, #160
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x30, [sp, #144]

    bl c_irq_handler

    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x30, [sp, #144]
    add sp, sp, #160
    eret

fiq_handler:
    sub sp, sp, #160
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x30, [sp, #144]

    bl c_fiq_handler

    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x30, [sp, #144]
    add sp, sp, #160
    eret

default_handler:
    bl c_default_handler
1:
    wfi
    b 1b
