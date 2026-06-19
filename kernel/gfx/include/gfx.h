#pragma once

#include <stdint.h>

void gfx_init(void);
void gfx_clear(uint32_t color);
void gfx_put_pixel(int x, int y, uint32_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);

void gfx_draw_char(int x, int y, char c, uint32_t color, int scale);
void gfx_draw_text(int x, int y, const char* text, uint32_t color, int scale);

void gfx_present(void);