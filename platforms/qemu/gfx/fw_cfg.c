#include <stdint.h>

#include "fw_cfg.h"
#include "arch.h"

#define FW_CFG_BASE 0x09020000UL

#define FW_CFG_DATA   0x00
#define FW_CFG_SELECT 0x08
#define FW_CFG_DMA    0x10

#define FW_CFG_FILE_DIR 0x0019

#define FW_CFG_DMA_CTL_ERROR  0x01
#define FW_CFG_DMA_CTL_READ   0x02
#define FW_CFG_DMA_CTL_SKIP   0x04
#define FW_CFG_DMA_CTL_SELECT 0x08
#define FW_CFG_DMA_CTL_WRITE  0x10

typedef struct {
    uint32_t control;
    uint32_t length;
    uint64_t address;
} __attribute__((packed, aligned(8))) FWCfgDmaAccess;

static uint16_t be16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}

static uint32_t be32(uint32_t x) {
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) << 8)  |
           ((x & 0x00FF0000U) >> 8)  |
           ((x & 0xFF000000U) >> 24);
}

static uint64_t be64(uint64_t x) {
    return ((uint64_t)be32((uint32_t)(x & 0xFFFFFFFFULL)) << 32) |
           ((uint64_t)be32((uint32_t)(x >> 32)));
}

static void memory_barrier(void) {
    arch_data_sync_barrier();
    arch_instruction_sync_barrier();
}

static void mmio_write16(uint64_t addr, uint16_t value) {
    *(volatile uint16_t*)addr = value;
}

static void mmio_write32(uint64_t addr, uint32_t value) {
    *(volatile uint32_t*)addr = value;
}

static uint8_t mmio_read8(uint64_t addr) {
    return *(volatile uint8_t*)addr;
}

static void fw_cfg_select(uint16_t selector) {
    mmio_write16(FW_CFG_BASE + FW_CFG_SELECT, be16(selector));
}

static uint8_t fw_cfg_read_u8(void) {
    return mmio_read8(FW_CFG_BASE + FW_CFG_DATA);
}

static uint16_t fw_cfg_read_be16(void) {
    uint16_t value = 0;

    value |= ((uint16_t)fw_cfg_read_u8()) << 8;
    value |= ((uint16_t)fw_cfg_read_u8()) << 0;

    return value;
}

static uint32_t fw_cfg_read_be32(void) {
    uint32_t value = 0;

    value |= ((uint32_t)fw_cfg_read_u8()) << 24;
    value |= ((uint32_t)fw_cfg_read_u8()) << 16;
    value |= ((uint32_t)fw_cfg_read_u8()) << 8;
    value |= ((uint32_t)fw_cfg_read_u8()) << 0;

    return value;
}

static int str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

int fw_cfg_find_file(const char* name, uint16_t* selector_out, uint32_t* size_out) {
    fw_cfg_select(FW_CFG_FILE_DIR);

    uint32_t count = fw_cfg_read_be32();

    if (count > 256) {
        return 0;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint32_t size = fw_cfg_read_be32();
        uint16_t select = fw_cfg_read_be16();

        fw_cfg_read_be16();

        char file_name[56];

        for (int j = 0; j < 56; j++) {
            file_name[j] = (char)fw_cfg_read_u8();
        }

        if (str_equal(file_name, name)) {
            if (selector_out) {
                *selector_out = select;
            }

            if (size_out) {
                *size_out = size;
            }

            return 1;
        }
    }

    return 0;
}

int fw_cfg_dma_write(uint16_t selector, const void* data, uint32_t size) {
    static volatile FWCfgDmaAccess dma;

    uint64_t dma_addr = (uint64_t)(uintptr_t)&dma;
    uint64_t data_addr = (uint64_t)(uintptr_t)data;

    uint32_t control =
        ((uint32_t)selector << 16) |
        FW_CFG_DMA_CTL_SELECT |
        FW_CFG_DMA_CTL_WRITE;

    dma.control = be32(control);
    dma.length = be32(size);
    dma.address = be64(data_addr);

    memory_barrier();

    mmio_write32(
        FW_CFG_BASE + FW_CFG_DMA,
        be32((uint32_t)(dma_addr >> 32))
    );

    mmio_write32(
        FW_CFG_BASE + FW_CFG_DMA + 4,
        be32((uint32_t)(dma_addr & 0xFFFFFFFFULL))
    );

    memory_barrier();

    while (1) {
        uint32_t status = be32(dma.control);

        if (status == 0) {
            return 1;
        }

        if (status & FW_CFG_DMA_CTL_ERROR) {
            return 0;
        }
    }
}