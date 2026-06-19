.section .text.boot
.global _start

_start:
    ldr x0, =stack_top
    mov sp, x0

    bl kernel_main

hang:
    wfe
    b hang

.section .bss
.align 16

stack_bottom:
    .skip 262144

stack_top: