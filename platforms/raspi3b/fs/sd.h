#pragma once

#include <stdint.h>

int sd_init(void);
int sd_read_sector(uint64_t sector, void* buffer);
int sd_write_sector(uint64_t sector, const void* buffer);
uint64_t sd_sector_count(void);
