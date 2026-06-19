#include <stdint.h>

#include "virtio_mmio.h"
#include "uart.h"

#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION     0x004
#define VIRTIO_MMIO_DEVICE_ID   0x008
#define VIRTIO_MMIO_VENDOR_ID   0x00C

#define VIRTIO_MAGIC 0x74726976U

#define VIRTIO_MMIO_BASE_START 0x0A000000UL
#define VIRTIO_MMIO_SLOT_SIZE  0x00000200UL
#define VIRTIO_MMIO_SLOT_COUNT 32

static uint32_t mmio_read32(uintptr_t addr) {
    return *(volatile uint32_t*)addr;
}

static void uart_put_hex_digit(uint32_t value) {
    value &= 0xF;

    if (value < 10) {
        uart_putc('0' + value);
    } else {
        uart_putc('A' + (value - 10));
    }
}

static void uart_put_hex32(uint32_t value) {
    uart_puts("0x");

    for (int i = 7; i >= 0; i--) {
        uart_put_hex_digit(value >> (i * 4));
    }
}

static void uart_put_hex64(uint64_t value) {
    uart_puts("0x");

    for (int i = 15; i >= 0; i--) {
        uart_put_hex_digit((uint32_t)(value >> (i * 4)));
    }
}

void virtio_mmio_debug_scan(void) {
    uart_puts("virtio-mmio scan\n");

    for (uint32_t i = 0; i < VIRTIO_MMIO_SLOT_COUNT; i++) {
        uintptr_t base = VIRTIO_MMIO_BASE_START + i * VIRTIO_MMIO_SLOT_SIZE;

        uint32_t magic = mmio_read32(base + VIRTIO_MMIO_MAGIC_VALUE);

        if (magic != VIRTIO_MAGIC) {
            continue;
        }

        uint32_t version = mmio_read32(base + VIRTIO_MMIO_VERSION);
        uint32_t device_id = mmio_read32(base + VIRTIO_MMIO_DEVICE_ID);
        uint32_t vendor_id = mmio_read32(base + VIRTIO_MMIO_VENDOR_ID);

        uart_puts("slot ");
        uart_put_hex32(i);

        uart_puts(" base ");
        uart_put_hex64((uint64_t)base);

        uart_puts(" version ");
        uart_put_hex32(version);

        uart_puts(" device_id ");
        uart_put_hex32(device_id);

        uart_puts(" vendor_id ");
        uart_put_hex32(vendor_id);

        uart_puts("\n");
    }
}

int virtio_mmio_find_device(uint32_t wanted_device_id, VirtioMMIODevice* out) {
    for (uint32_t i = 0; i < VIRTIO_MMIO_SLOT_COUNT; i++) {
        uintptr_t base = VIRTIO_MMIO_BASE_START + i * VIRTIO_MMIO_SLOT_SIZE;

        uint32_t magic = mmio_read32(base + VIRTIO_MMIO_MAGIC_VALUE);

        if (magic != VIRTIO_MAGIC) {
            continue;
        }

        uint32_t device_id = mmio_read32(base + VIRTIO_MMIO_DEVICE_ID);

        if (device_id != wanted_device_id) {
            continue;
        }

        if (out) {
            out->base = base;
            out->device_id = device_id;
            out->vendor_id = mmio_read32(base + VIRTIO_MMIO_VENDOR_ID);
        }

        return 1;
    }

    return 0;
}