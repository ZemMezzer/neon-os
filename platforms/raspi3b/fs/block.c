#include <stdint.h>

#include "block.h"
#include "sd.h"

int block_init(void) {
    return sd_init();
}

int block_read(uint64_t sector, void* buffer, uint32_t count) {
    uint8_t* out = (uint8_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        if (sd_read_sector(sector + i, out + i * BLOCK_SECTOR_SIZE) != 0) {
            return -1;
        }
    }

    return 0;
}

int block_write(uint64_t sector, const void* buffer, uint32_t count) {
    const uint8_t* in = (const uint8_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        if (sd_write_sector(sector + i, in + i * BLOCK_SECTOR_SIZE) != 0) {
            return -1;
        }
    }

    return 0;
}

uint64_t block_sector_count(void) {
    return sd_sector_count();
}
