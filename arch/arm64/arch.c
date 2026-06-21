#include "arch.h"

void arch_wait_for_event(void) {
    asm volatile("wfe" ::: "memory");
}

void arch_data_sync_barrier(void) {
    asm volatile("dsb sy" ::: "memory");
}

void arch_instruction_sync_barrier(void) {
    asm volatile("isb" ::: "memory");
}

uint64_t arch_read_counter(void) {
    uint64_t value;

    /* ARM64 system register: CNTPCT_EL0. */
    asm volatile(
        "mrs %0, cntpct_el0"
        : "=r"(value)
    );

    return value;
}

uint64_t arch_get_counter_frequency(void) {
    uint64_t value;

    /* ARM64 system register: CNTFRQ_EL0. */
    asm volatile(
        "mrs %0, cntfrq_el0"
        : "=r"(value)
    );

    return value;
}
