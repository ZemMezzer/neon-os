#pragma once

#include <stdint.h>

#define FB_WIDTH  800
#define FB_HEIGHT 600

typedef struct Framebuffer {
    uint32_t* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} Framebuffer;

Framebuffer* framebuffer_init(void);
void framebuffer_present(void);