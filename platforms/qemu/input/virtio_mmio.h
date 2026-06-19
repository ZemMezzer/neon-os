#pragma once

#include <stdint.h>

typedef struct VirtioMMIODevice {
    uintptr_t base;
    uint32_t device_id;
    uint32_t vendor_id;
} VirtioMMIODevice;

int virtio_mmio_find_device(uint32_t wanted_device_id, VirtioMMIODevice* out);
void virtio_mmio_debug_scan(void);