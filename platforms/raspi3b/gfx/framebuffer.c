#include <stdint.h>
#include <stddef.h>

#include "framebuffer.h"
#include "arch.h"

#define PERIPHERAL_BASE 0x3F000000UL
#define MBOX_BASE       (PERIPHERAL_BASE + 0x00B880UL)

#define MBOX_READ       0x00
#define MBOX_STATUS     0x18
#define MBOX_WRITE      0x20

#define MBOX_STATUS_FULL  (1u << 31)
#define MBOX_STATUS_EMPTY (1u << 30)
#define MBOX_CHANNEL_PROPERTY 8u

#define MBOX_RESPONSE_SUCCESS 0x80000000u
#define MBOX_TAG_RESPONSE     0x80000000u

#define TAG_SET_PHYSICAL_SIZE 0x00048003u
#define TAG_SET_VIRTUAL_SIZE  0x00048004u
#define TAG_SET_VIRTUAL_OFFSET 0x00048009u
#define TAG_SET_DEPTH         0x00048005u
#define TAG_SET_PIXEL_ORDER   0x00048006u
#define TAG_ALLOCATE_BUFFER   0x00040001u
#define TAG_GET_PITCH         0x00040008u

/* Pi 2/3 ARM physical addresses are exposed to VideoCore through this alias. */
#define VC_BUS_ALIAS 0xC0000000u
#define VC_BUS_MASK  0x3FFFFFFFu

static uint32_t backbuffer[FB_WIDTH * FB_HEIGHT]
    __attribute__((aligned(4096)));

static volatile uint32_t* visible_pixels = 0;
static uint32_t visible_pitch_pixels = 0;
static int framebuffer_available = 0;

static Framebuffer framebuffer = {
    .pixels = backbuffer,
    .width = FB_WIDTH,
    .height = FB_HEIGHT,
    .pitch = FB_WIDTH
};

/*
 * The mailbox message must be 16-byte aligned. The VideoCore firmware
 * overwrites the request words with its response.
 */
static volatile uint32_t property_message[35] __attribute__((aligned(16)));

static inline void mmio_write(uintptr_t address, uint32_t value) {
    *(volatile uint32_t*)address = value;
}

static inline uint32_t mmio_read(uintptr_t address) {
    return *(volatile uint32_t*)address;
}

static void memory_barrier(void) {
    arch_data_sync_barrier();
    arch_instruction_sync_barrier();
}

static uint32_t arm_to_vc_bus(const void* pointer) {
    return ((uint32_t)(uintptr_t)pointer & VC_BUS_MASK) | VC_BUS_ALIAS;
}

static uintptr_t vc_bus_to_arm(uint32_t address) {
    return (uintptr_t)(address & VC_BUS_MASK);
}

static int mailbox_property_call(volatile uint32_t* message) {
    uint32_t request = arm_to_vc_bus((const void*)message) | MBOX_CHANNEL_PROPERTY;

    memory_barrier();

    while (mmio_read(MBOX_BASE + MBOX_STATUS) & MBOX_STATUS_FULL) {
    }

    mmio_write(MBOX_BASE + MBOX_WRITE, request);

    for (;;) {
        uint32_t response;

        while (mmio_read(MBOX_BASE + MBOX_STATUS) & MBOX_STATUS_EMPTY) {
        }

        response = mmio_read(MBOX_BASE + MBOX_READ);

        if ((response & 0xFu) != MBOX_CHANNEL_PROPERTY) {
            continue;
        }

        if ((response & ~0xFu) != (request & ~0xFu)) {
            continue;
        }

        memory_barrier();
        return message[1] == MBOX_RESPONSE_SUCCESS;
    }
}

static int raspi_framebuffer_setup(void) {
    volatile uint32_t* m = property_message;
    uint32_t framebuffer_bus_address;
    uint32_t pitch_bytes;

    m[0] = sizeof(property_message);
    m[1] = 0;

    m[2] = TAG_SET_PHYSICAL_SIZE;
    m[3] = 8;
    m[4] = 8;
    m[5] = FB_WIDTH;
    m[6] = FB_HEIGHT;

    m[7] = TAG_SET_VIRTUAL_SIZE;
    m[8] = 8;
    m[9] = 8;
    m[10] = FB_WIDTH;
    m[11] = FB_HEIGHT;

    m[12] = TAG_SET_VIRTUAL_OFFSET;
    m[13] = 8;
    m[14] = 8;
    m[15] = 0;
    m[16] = 0;

    m[17] = TAG_SET_DEPTH;
    m[18] = 4;
    m[19] = 4;
    m[20] = 32;

    m[21] = TAG_SET_PIXEL_ORDER;
    m[22] = 4;
    m[23] = 4;
    m[24] = 0; /* BGR byte order preserves NeonOS 0x00RRGGBB pixels. */

    m[25] = TAG_ALLOCATE_BUFFER;
    m[26] = 8;
    m[27] = 4;
    m[28] = 4096;
    m[29] = 0;

    m[30] = TAG_GET_PITCH;
    m[31] = 4;
    m[32] = 0;
    m[33] = 0;

    m[34] = 0;

    if (!mailbox_property_call(m)) {
        return 0;
    }

    if ((m[27] & MBOX_TAG_RESPONSE) == 0 ||
        (m[32] & MBOX_TAG_RESPONSE) == 0) {
        return 0;
    }

    framebuffer_bus_address = m[28];
    pitch_bytes = m[33];

    if (framebuffer_bus_address == 0 || pitch_bytes < FB_WIDTH * sizeof(uint32_t)) {
        return 0;
    }

    visible_pixels = (volatile uint32_t*)vc_bus_to_arm(framebuffer_bus_address);
    visible_pitch_pixels = pitch_bytes / sizeof(uint32_t);

    return visible_pitch_pixels >= FB_WIDTH;
}

Framebuffer* framebuffer_init(void) {
    framebuffer_available = raspi_framebuffer_setup();
    return &framebuffer;
}

void framebuffer_present(void) {
    if (!framebuffer_available || visible_pixels == 0) {
        return;
    }

    for (uint32_t y = 0; y < FB_HEIGHT; y++) {
        volatile uint32_t* destination = visible_pixels + y * visible_pitch_pixels;
        const uint32_t* source = backbuffer + y * FB_WIDTH;

        for (uint32_t x = 0; x < FB_WIDTH; x++) {
            destination[x] = source[x];
        }
    }

    memory_barrier();
}
