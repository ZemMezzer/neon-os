#include <stdint.h>

#include "gfx.h"
#include "framebuffer.h"
#include "lua_gfx.h"
#include "lua_input.h"

#include "lauxlib.h"


#define LUA_GFX_COORDINATE_LIMIT 4096
#define LUA_GFX_SCALE_LIMIT 32


static int lua_gfx_check_coordinate(
    lua_State* state,
    int argument_index
) {
    lua_Integer value;

    value = luaL_checkinteger(state, argument_index);

    if (
        value < -LUA_GFX_COORDINATE_LIMIT ||
        value > LUA_GFX_COORDINATE_LIMIT
    ) {
        luaL_argerror(
            state,
            argument_index,
            "coordinate is outside the supported drawing range"
        );
    }

    return (int)value;
}


static int lua_gfx_check_size(
    lua_State* state,
    int argument_index
) {
    lua_Integer value;

    value = luaL_checkinteger(state, argument_index);

    if (value < 0 || value > LUA_GFX_COORDINATE_LIMIT) {
        luaL_argerror(
            state,
            argument_index,
            "size is outside the supported drawing range"
        );
    }

    return (int)value;
}


static int lua_gfx_check_scale(
    lua_State* state,
    int argument_index
) {
    lua_Integer value;

    value = luaL_optinteger(state, argument_index, 1);

    if (value < 1 || value > LUA_GFX_SCALE_LIMIT) {
        luaL_argerror(
            state,
            argument_index,
            "scale must be between 1 and 32"
        );
    }

    return (int)value;
}


static uint32_t lua_gfx_check_color(
    lua_State* state,
    int argument_index
) {
    lua_Integer value;

    value = luaL_checkinteger(state, argument_index);

    if (value < 0 || value > 0xFFFFFFFFLL) {
        luaL_argerror(
            state,
            argument_index,
            "color must be an integer from 0x000000 to 0xFFFFFFFF"
        );
    }

    return (uint32_t)value;
}


static int lua_gfx_absolute(int value) {
    return value < 0 ? -value : value;
}


static void lua_gfx_draw_line(
    int x0,
    int y0,
    int x1,
    int y1,
    uint32_t color
) {
    int delta_x;
    int delta_y;
    int step_x;
    int step_y;
    int error;

    delta_x = lua_gfx_absolute(x1 - x0);
    delta_y = -lua_gfx_absolute(y1 - y0);

    step_x = x0 < x1 ? 1 : -1;
    step_y = y0 < y1 ? 1 : -1;

    error = delta_x + delta_y;

    for (;;) {
        int doubled_error;

        gfx_put_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        doubled_error = error * 2;

        if (doubled_error >= delta_y) {
            error += delta_y;
            x0 += step_x;
        }

        if (doubled_error <= delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }
}


static int lua_gfx_width(lua_State* state) {
    lua_pushinteger(state, FB_WIDTH);
    return 1;
}


static int lua_gfx_height(lua_State* state) {
    lua_pushinteger(state, FB_HEIGHT);
    return 1;
}


static int lua_gfx_clear(lua_State* state) {
    uint32_t color;

    color = lua_gfx_check_color(state, 1);
    gfx_clear(color);

    return 0;
}


static int lua_gfx_pixel(lua_State* state) {
    int x;
    int y;
    uint32_t color;

    x = lua_gfx_check_coordinate(state, 1);
    y = lua_gfx_check_coordinate(state, 2);
    color = lua_gfx_check_color(state, 3);

    gfx_put_pixel(x, y, color);

    return 0;
}


static int lua_gfx_line(lua_State* state) {
    int x0;
    int y0;
    int x1;
    int y1;
    uint32_t color;

    x0 = lua_gfx_check_coordinate(state, 1);
    y0 = lua_gfx_check_coordinate(state, 2);
    x1 = lua_gfx_check_coordinate(state, 3);
    y1 = lua_gfx_check_coordinate(state, 4);
    color = lua_gfx_check_color(state, 5);

    lua_gfx_draw_line(x0, y0, x1, y1, color);

    return 0;
}


static int lua_gfx_rect(lua_State* state) {
    int x;
    int y;
    int width;
    int height;
    int right;
    int bottom;
    uint32_t color;

    x = lua_gfx_check_coordinate(state, 1);
    y = lua_gfx_check_coordinate(state, 2);
    width = lua_gfx_check_size(state, 3);
    height = lua_gfx_check_size(state, 4);
    color = lua_gfx_check_color(state, 5);

    if (width == 0 || height == 0) {
        return 0;
    }

    right = x + width - 1;
    bottom = y + height - 1;

    lua_gfx_draw_line(x, y, right, y, color);
    lua_gfx_draw_line(right, y, right, bottom, color);
    lua_gfx_draw_line(right, bottom, x, bottom, color);
    lua_gfx_draw_line(x, bottom, x, y, color);

    return 0;
}


static int lua_gfx_fill_rect(lua_State* state) {
    int x;
    int y;
    int width;
    int height;
    uint32_t color;

    x = lua_gfx_check_coordinate(state, 1);
    y = lua_gfx_check_coordinate(state, 2);
    width = lua_gfx_check_size(state, 3);
    height = lua_gfx_check_size(state, 4);
    color = lua_gfx_check_color(state, 5);

    gfx_fill_rect(x, y, width, height, color);

    return 0;
}


static int lua_gfx_text(lua_State* state) {
    int x;
    int y;
    const char* text;
    uint32_t color;
    int scale;

    x = lua_gfx_check_coordinate(state, 1);
    y = lua_gfx_check_coordinate(state, 2);
    text = luaL_checkstring(state, 3);
    color = lua_gfx_check_color(state, 4);
    scale = lua_gfx_check_scale(state, 5);

    gfx_draw_text(x, y, text, color, scale);

    return 0;
}


static int lua_gfx_present(lua_State* state) {
    (void)state;

    gfx_present();

    /* Input may refresh once again on the next visual frame. */
    lua_input_frame_presented();

    return 0;
}


static const luaL_Reg lua_gfx_functions[] = {
    { "width",     lua_gfx_width },
    { "height",    lua_gfx_height },
    { "clear",     lua_gfx_clear },
    { "pixel",     lua_gfx_pixel },
    { "line",      lua_gfx_line },
    { "rect",      lua_gfx_rect },
    { "fill_rect", lua_gfx_fill_rect },
    { "text",      lua_gfx_text },
    { "present",   lua_gfx_present },
    { NULL,        NULL }
};


int luaopen_gfx(lua_State* state) {
    luaL_newlib(state, lua_gfx_functions);
    return 1;
}
