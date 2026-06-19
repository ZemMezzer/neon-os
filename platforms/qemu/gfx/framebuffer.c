#include <stdint.h>

#include "framebuffer.h"
#include "fw_cfg.h"

#define DRM_FORMAT_XRGB8888 0x34325258U

typedef struct {
    uint32_t addr_hi;
    uint32_t addr_lo;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} RAMFBCfg;


static volatile uint32_t qemu_framebuffer[FB_WIDTH * FB_HEIGHT]
    __attribute__((aligned(4096)));


static uint32_t backbuffer[FB_WIDTH * FB_HEIGHT]
    __attribute__((aligned(4096)));

static Framebuffer framebuffer = {
    .pixels = backbuffer,
    .width = FB_WIDTH,
    .height = FB_HEIGHT,
    .pitch = FB_WIDTH
};

static volatile RAMFBCfg ramfb_cfg __attribute__((aligned(8)));

static uint16_t ramfb_selector = 0;
static int ramfb_available = 0;

static uint32_t be32(uint32_t x) {
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) << 8)  |
           ((x & 0x00FF0000U) >> 8)  |
           ((x & 0xFF000000U) >> 24);
}

static void memory_barrier(void) {
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static void ramfb_build_config(void) {
    uint64_t fb_addr = (uint64_t)(uintptr_t)&qemu_framebuffer[0];

    ramfb_cfg.addr_hi = be32((uint32_t)(fb_addr >> 32));
    memory_barrier();

    ramfb_cfg.addr_lo = be32((uint32_t)(fb_addr & 0xFFFFFFFFULL));
    memory_barrier();

    ramfb_cfg.fourcc = be32(DRM_FORMAT_XRGB8888);
    memory_barrier();

    ramfb_cfg.flags = be32(0);
    memory_barrier();

    ramfb_cfg.width = be32(FB_WIDTH);
    memory_barrier();

    ramfb_cfg.height = be32(FB_HEIGHT);
    memory_barrier();

    ramfb_cfg.stride = be32(FB_WIDTH * 4);
    memory_barrier();
}

static void copy_backbuffer_to_visible(void) {
    for (uint32_t i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        qemu_framebuffer[i] = backbuffer[i];
    }
}

static int ramfb_send_config(void) {
    if (!ramfb_available) {
        return 0;
    }

    memory_barrier();

    int ok = fw_cfg_dma_write(
        ramfb_selector,
        (const void*)(uintptr_t)&ramfb_cfg,
        sizeof(RAMFBCfg)
    );

    memory_barrier();

    return ok;
}

Framebuffer* framebuffer_init(void) {
    uint32_t ramfb_size = 0;

    ramfb_available = fw_cfg_find_file(
        "etc/ramfb",
        &ramfb_selector,
        &ramfb_size
    );

    ramfb_build_config();

    return &framebuffer;
}

void framebuffer_present(void) {
    copy_backbuffer_to_visible();
    memory_barrier();
    ramfb_send_config();
    memory_barrier();
}