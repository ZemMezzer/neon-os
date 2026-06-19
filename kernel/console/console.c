#include "console.h"
#include "gfx.h"
#include "framebuffer.h"

#define CONSOLE_BG 0x00000000
#define CONSOLE_FG 0x00FFFFFF

#define CHAR_SCALE 2
#define CHAR_WIDTH  (6 * CHAR_SCALE)
#define CHAR_HEIGHT (8 * CHAR_SCALE)

static int cursor_x = 0;
static int cursor_y = 0;

static int columns = FB_WIDTH / CHAR_WIDTH;
static int rows = FB_HEIGHT / CHAR_HEIGHT;

void console_init(void) {
    console_clear();
}

void console_clear(void) {
    gfx_clear(CONSOLE_BG);
    cursor_x = 0;
    cursor_y = 0;
    gfx_present();
}

static void console_newline(void) {
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= rows) {
        console_clear();
    }
}

void console_putc(char c) {
    if (c == '\n') {
        console_newline();
        gfx_present();
        return;
    }

    if (c == '\r') {
        cursor_x = 0;
        return;
    }

    gfx_draw_char(
        cursor_x * CHAR_WIDTH,
        cursor_y * CHAR_HEIGHT,
        c,
        CONSOLE_FG,
        CHAR_SCALE
    );

    cursor_x++;

    if (cursor_x >= columns) {
        console_newline();
    }

    gfx_present();
}

void console_write(const char* text) {
    while (*text) {
        console_putc(*text);
        text++;
    }
}

void console_backspace(void) {
    if (cursor_x <= 0) {
        return;
    }

    cursor_x--;

    gfx_fill_rect(
        cursor_x * CHAR_WIDTH,
        cursor_y * CHAR_HEIGHT,
        CHAR_WIDTH,
        CHAR_HEIGHT,
        CONSOLE_BG
    );

    gfx_present();
}