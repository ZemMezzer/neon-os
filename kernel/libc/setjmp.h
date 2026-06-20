#pragma once

#include <stdint.h>

typedef struct __attribute__((aligned(16))) NeonJmpContext {
    uint64_t x19_x30[12];
    uint64_t sp;
    uint64_t reserved;
    uint64_t d8_d15[8];
} NeonJmpContext;

typedef NeonJmpContext jmp_buf[1];

int setjmp(jmp_buf env)
    __attribute__((returns_twice));

void longjmp(jmp_buf env, int value)
    __attribute__((noreturn));