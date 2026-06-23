.section .text.boot
.align 7
.global _start
.type _start, %function

.extern kernel_main
.extern uart_init

_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbz x0, 1f

0:
    wfe
    b 0b

1:
    msr daifset, #0xf

    ldr x0, =stack_top
    mov sp, x0

    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)
    msr cpacr_el1, x0
    isb

    bl uart_init
    bl kernel_main

2:
    wfe
    b 2b

.size _start, . - _start

.section .bss
.align 16

stack_bottom:
    .skip 262144

stack_top:
