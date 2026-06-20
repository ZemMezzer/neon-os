#pragma once

#include <stdint.h>

#define BLOCK_SECTOR_SIZE 512

int block_init(void);

int block_read(uint64_t sector, void* buffer, uint32_t count);
int block_write(uint64_t sector, const void* buffer, uint32_t count);

uint64_t block_sector_count(void);