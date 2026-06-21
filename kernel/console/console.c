#include "console.h"
#include "framebuffer.h"
#include "uart.h"

#if GFX_ENABLED
    #include "gfx.h"
#endif

#define CONSOLE_BG 0x00000000
#define CONSOLE_FG 0x00FFFFFF

#define CHAR_SCALE 2
#define CHAR_WIDTH  (6 * CHAR_SCALE)
#define CHAR_HEIGHT (8 * CHAR_SCALE)

#define CONSOLE_CURSOR_BLINK_TICKS 500000
#define CONSOLE_CURSOR_HEIGHT 2

static int cursor_x = 0;
static int cursor_y = 0;

static int columns = FB_WIDTH / CHAR_WIDTH;
static int rows = FB_HEIGHT / CHAR_HEIGHT;

static int console_cursor_enabled = 0;
static int console_cursor_visible = 0;
static unsigned int console_cursor_ticks = 0;

static int console_cursor_drawn = 0;
static int console_cursor_drawn_x = 0;
static int console_cursor_drawn_y = 0;

static int console_suspended = 0;
static int suspended_cursor_visible = 0;

static void console_draw_cursor_at(int cell_x, int cell_y, unsigned int color) {
    int px;
    int py;

    if (cell_x < 0 || cell_y < 0) {
        return;
    }

    if (cell_x >= columns || cell_y >= rows) {
        return;
    }

    px = cell_x * CHAR_WIDTH;
    py = cell_y * CHAR_HEIGHT + CHAR_HEIGHT - CONSOLE_CURSOR_HEIGHT;

#if GFX_ENABLED
    gfx_fill_rect(
        px,
        py,
        CHAR_WIDTH,
        CONSOLE_CURSOR_HEIGHT,
        color
    );
#endif
}

static void console_erase_drawn_cursor(void) {
    if (!console_cursor_drawn) {
        return;
    }

    console_draw_cursor_at(
        console_cursor_drawn_x,
        console_cursor_drawn_y,
        CONSOLE_BG
    );

    console_cursor_drawn = 0;
}

static void console_draw_current_cursor(void) {
    if (!console_cursor_enabled) {
        return;
    }

    if (!console_cursor_visible) {
        return;
    }

    console_draw_cursor_at(cursor_x, cursor_y, CONSOLE_FG);

    console_cursor_drawn = 1;
    console_cursor_drawn_x = cursor_x;
    console_cursor_drawn_y = cursor_y;
}

static void console_redraw_cursor_if_needed(void) {
    if (console_cursor_enabled && console_cursor_visible) {
        console_draw_current_cursor();
    }
}

void console_init(void) {
    console_clear();
}

void console_clear(void) {
    if (console_suspended) {
        return;
    }
#if GFX_ENABLED
    gfx_clear(CONSOLE_BG);
#endif

    cursor_x = 0;
    cursor_y = 0;

    console_cursor_drawn = 0;

    console_redraw_cursor_if_needed();

#if GFX_ENABLED
    gfx_present();
#endif
}

void console_suspend(void) {
    if (console_suspended) {
        return;
    }

    console_erase_drawn_cursor();

    suspended_cursor_visible = console_cursor_visible;
    console_cursor_visible = 0;
    console_cursor_ticks = 0;
    console_cursor_drawn = 0;

    console_suspended = 1;

    cursor_x = 0;
    cursor_y = 0;

#if GFX_ENABLED
    gfx_clear(CONSOLE_BG);
    gfx_present();
#endif
}

void console_resume(void) {
    if (!console_suspended) {
        return;
    }

    console_suspended = 0;

    cursor_x = 0;
    cursor_y = 0;
    console_cursor_drawn = 0;
    console_cursor_ticks = 0;

    console_cursor_visible = (
        console_cursor_enabled && suspended_cursor_visible
    ) ? 1 : 0;
    suspended_cursor_visible = 0;

#if GFX_ENABLED
    gfx_clear(CONSOLE_BG);
    console_redraw_cursor_if_needed();
    gfx_present();
#endif
}

void console_cursor_enable(int enabled) {
    if (console_suspended) {
        return;
    }

    console_erase_drawn_cursor();

    console_cursor_enabled = enabled ? 1 : 0;
    console_cursor_visible = enabled ? 1 : 0;
    console_cursor_ticks = 0;

    console_redraw_cursor_if_needed();
#if GFX_ENABLED
    gfx_present();
#endif
}

void console_cursor_show(void) {
    if (console_suspended || !console_cursor_enabled) {
        return;
    }

    console_erase_drawn_cursor();

    console_cursor_visible = 1;
    console_cursor_ticks = 0;

    console_draw_current_cursor();

#if GFX_ENABLED
    gfx_present();
#endif
}

void console_cursor_hide(void) {
    if (console_suspended) {
        return;
    }

    console_erase_drawn_cursor();

    console_cursor_visible = 0;
    console_cursor_ticks = 0;

#if GFX_ENABLED
    gfx_present();
#endif
}

void console_cursor_update(void) {
    if (console_suspended || !console_cursor_enabled) {
        return;
    }

    console_cursor_ticks++;

    if (console_cursor_ticks < CONSOLE_CURSOR_BLINK_TICKS) {
        return;
    }

    console_cursor_ticks = 0;

    if (console_cursor_visible) {
        console_erase_drawn_cursor();
        console_cursor_visible = 0;
    } else {
        console_cursor_visible = 1;
        console_draw_current_cursor();
    }

#if GFX_ENABLED
    gfx_present();
#endif
}

static void console_newline(void) {
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= rows) {
#if GFX_ENABLED
        gfx_clear(CONSOLE_BG);
#endif

        cursor_x = 0;
        cursor_y = 0;

        console_cursor_drawn = 0;
    }
}


void console_putc(char c) {
    uart_putc(c);

    if (console_suspended) {
        return;
    }

    console_erase_drawn_cursor();

    if (c == '\n') {
        console_newline();
        console_redraw_cursor_if_needed();
#if GFX_ENABLED
        gfx_present();
#endif
        return;
    }

    if (c == '\r') {
        cursor_x = 0;
        console_redraw_cursor_if_needed();
#if GFX_ENABLED
        gfx_present();
#endif
        return;
    }

#if GFX_ENABLED
    gfx_draw_char(
        cursor_x * CHAR_WIDTH,
        cursor_y * CHAR_HEIGHT,
        c,
        CONSOLE_FG,
        CHAR_SCALE
    );
#endif

    cursor_x++;

    if (cursor_x >= columns) {
        console_newline();
    }

    console_redraw_cursor_if_needed();

#if GFX_ENABLED
    gfx_present();
#endif
}

void console_write(const char* text) {
    if (!text) {
        return;
    }

    while (*text) {
        console_putc(*text);
        text++;
    }
}

void console_backspace(void) {
    if (console_suspended) {
        return;
    }

    console_erase_drawn_cursor();

    if (cursor_x <= 0) {
        console_redraw_cursor_if_needed();
#if GFX_ENABLED
        gfx_present();
#endif
        return;
    }

    cursor_x--;

    uart_putc('\b');
    uart_putc(' ');
    uart_putc('\b');

#if GFX_ENABLED
    gfx_fill_rect(
        cursor_x * CHAR_WIDTH,
        cursor_y * CHAR_HEIGHT,
        CHAR_WIDTH,
        CHAR_HEIGHT,
        CONSOLE_BG
    );
#endif

    console_redraw_cursor_if_needed();

#if GFX_ENABLED
    gfx_present();
#endif
}

int console_get_cursor_x(void) {
    return cursor_x;
}

int console_get_cursor_y(void) {
    return cursor_y;
}

int console_get_columns(void) {
    return columns;
}

void console_set_cursor_pos(int x, int y) {
    if (console_suspended) {
        return;
    }

    console_erase_drawn_cursor();

    if (x < 0) {
        x = 0;
    }

    if (y < 0) {
        y = 0;
    }

    if (x >= columns) {
        x = columns - 1;
    }

    if (y >= rows) {
        y = rows - 1;
    }

    cursor_x = x;
    cursor_y = y;

    console_redraw_cursor_if_needed();

#if GFX_ENABLED
    gfx_present();
#endif
}

void console_clear_line_from_cursor(void) {
    int px;
    int py;
    int width;

    if (console_suspended) {
        return;
    }

    console_erase_drawn_cursor();

    px = cursor_x * CHAR_WIDTH;
    py = cursor_y * CHAR_HEIGHT;
    width = FB_WIDTH - px;

    if (width > 0) {
#if GFX_ENABLED
        gfx_fill_rect(
            px,
            py,
            width,
            CHAR_HEIGHT,
            CONSOLE_BG
        );
#endif
    }

    console_redraw_cursor_if_needed();

#if GFX_ENABLED
    gfx_present();
#endif
}
