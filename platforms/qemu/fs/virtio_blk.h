#pragma once

#include <stdint.h>

int virtio_blk_init(void);

int virtio_blk_read_sector(uint64_t sector, void* buffer);
int virtio_blk_write_sector(uint64_t sector, const void* buffer);

uint64_t virtio_blk_sector_count(void);