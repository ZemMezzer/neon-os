.section .text.boot
.global _start

_start:
    ldr x0, =stack_top
    mov sp, x0

    bl kernel_main

hang:
    wfe
    b hang


.section .stack, "aw", %nobits
.align 16

.global stack_bottom
.global stack_top

stack_bottom:
    .skip 262144

stack_top: