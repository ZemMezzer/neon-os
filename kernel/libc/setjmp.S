.section .text
.align 2

.global setjmp
.type setjmp, %function

setjmp:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]

    mov x1, sp
    str x1, [x0, #96]

    str xzr, [x0, #104]

    stp d8,  d9,  [x0, #112]
    stp d10, d11, [x0, #128]
    stp d12, d13, [x0, #144]
    stp d14, d15, [x0, #160]

    mov w0, wzr
    ret

.size setjmp, . - setjmp


.global longjmp
.type longjmp, %function

longjmp:
    /*
        C requires longjmp(env, 0) to make setjmp return 1.
    */
    cbnz w1, 1f
    mov w1, #1

1:
    /*
        Keep saved SP in x2 before restoring registers.
    */
    ldr x2, [x0, #96]

    ldp d8,  d9,  [x0, #112]
    ldp d10, d11, [x0, #128]
    ldp d12, d13, [x0, #144]
    ldp d14, d15, [x0, #160]

    ldp x19, x20, [x0, #0]
    ldp x21, x22, [x0, #16]
    ldp x23, x24, [x0, #32]
    ldp x25, x26, [x0, #48]
    ldp x27, x28, [x0, #64]
    ldp x29, x30, [x0, #80]

    mov sp, x2
    mov w0, w1
    ret

.size longjmp, . - longjmp