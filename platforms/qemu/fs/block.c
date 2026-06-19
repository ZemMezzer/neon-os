#include <stdint.h>

#include "block.h"
#include "virtio_blk.h"

int block_init(void) {
    return virtio_blk_init();
}

int block_read(uint64_t sector, void* buffer, uint32_t count) {
    uint8_t* out = (uint8_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        if (virtio_blk_read_sector(sector + i, out + i * BLOCK_SECTOR_SIZE) != 0) {
            return -1;
        }
    }

    return 0;
}

int block_write(uint64_t sector, const void* buffer, uint32_t count) {
    const uint8_t* in = (const uint8_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        if (virtio_blk_write_sector(sector + i, in + i * BLOCK_SECTOR_SIZE) != 0) {
            return -1;
        }
    }

    return 0;
}

uint64_t block_sector_count(void) {
    return virtio_blk_sector_count();
}