.section .text.boot
.global _start

_start:
    ldr x0, =stack_top
    mov sp, x0

    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)
    msr cpacr_el1, x0
    isb

    bl kernel_main

hang:
    wfe
    b hang


.section .bss
.align 16

stack_bottom:
    .skip 262144

stack_top: