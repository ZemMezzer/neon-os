#include <stdint.h>

void arch_wait_for_event(void);
void arch_data_sync_barrier(void);
void arch_instruction_sync_barrier(void);
uint64_t arch_read_counter(void);
uint64_t arch_get_counter_frequency(void);