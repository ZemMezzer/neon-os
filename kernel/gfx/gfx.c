#include "gfx.h"
#include "font.h"
#include "framebuffer.h"

static Framebuffer* fb = 0;

void gfx_init(void) {
    fb = framebuffer_init();
}

void gfx_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            fb->pixels[y * fb->pitch + x] = color;
        }
    }
}

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0) return;
    if (x >= (int)fb->width) return;
    if (y >= (int)fb->height) return;

    fb->pixels[y * fb->pitch + x] = color;
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            gfx_put_pixel(x + px, y + py, color);
        }
    }
}

void gfx_draw_char(int x, int y, char c, uint32_t color, int scale) {
    if (scale < 1) {
        scale = 1;
    }

    const uint8_t* glyph = font_get_glyph(c);

    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];

        for (int row = 0; row < 8; row++) {
            if (line & (1 << row)) {
                gfx_fill_rect(
                    x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

void gfx_draw_text(int x, int y, const char* text, uint32_t color, int scale) {
    if (scale < 1) {
        scale = 1;
    }

    while (*text) {
        gfx_draw_char(x, y, *text, color, scale);

        x += 6 * scale;

        text++;
    }
}

void gfx_present(void) {
    framebuffer_present();
}