#include "lua_bitmap.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "ff.h"
#include "gfx.h"

#define BITMAP_METATABLE "NeonOS.bitmap"

typedef struct BitmapImage {
    uint16_t width;
    uint16_t height;
    uint32_t* pixels;
} BitmapImage;

static uint16_t bitmap_read_u16_le(const unsigned char* data) {
    return
        (uint16_t)data[0] |
        ((uint16_t)data[1] << 8);
}

static uint32_t bitmap_read_u32_le(const unsigned char* data) {
    return
        (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static BitmapImage* bitmap_check(lua_State* state, int index) {
    return (BitmapImage*)luaL_checkudata(state, index, BITMAP_METATABLE);
}

static int bitmap_gc(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);

    if (image->pixels != NULL) {
        free(image->pixels);
        image->pixels = NULL;
    }

    image->width = 0;
    image->height = 0;
    return 0;
}

static int bitmap_load_pkicn(lua_State* state) {
    const char* path = luaL_checkstring(state, 1);
    FIL file;
    FRESULT result;
    unsigned char header[12];
    UINT read_count = 0;
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint16_t flags;
    size_t pixel_count;
    size_t pixel_bytes;
    BitmapImage* image;

    result = f_open(&file, path, FA_READ);

    if (result != FR_OK) {
        lua_pushnil(state);
        lua_pushfstring(state, "cannot open file: %s", path);
        return 2;
    }

    result = f_read(&file, header, sizeof(header), &read_count);

    if (result != FR_OK || read_count != sizeof(header)) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushstring(state, "cannot read PKICN header");
        return 2;
    }

    if (
        header[0] != 'P' ||
        header[1] != 'K' ||
        header[2] != 'I' ||
        header[3] != 'C'
    ) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushstring(state, "invalid PKICN magic");
        return 2;
    }

    version = bitmap_read_u16_le(header + 4);
    width = bitmap_read_u16_le(header + 6);
    height = bitmap_read_u16_le(header + 8);
    flags = bitmap_read_u16_le(header + 10);

    if (version != 1) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushfstring(state, "unsupported PKICN version: %d", (int)version);
        return 2;
    }

    if (width == 0 || height == 0) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushstring(state, "invalid image size");
        return 2;
    }

    if (flags != 1) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushfstring(state, "unsupported PKICN flags: %d", (int)flags);
        return 2;
    }

    pixel_count = (size_t)width * (size_t)height;
    pixel_bytes = pixel_count * sizeof(uint32_t);

    image = (BitmapImage*)lua_newuserdatauv(state, sizeof(BitmapImage), 0);
    image->width = width;
    image->height = height;
    image->pixels = NULL;

    luaL_getmetatable(state, BITMAP_METATABLE);
    lua_setmetatable(state, -2);

    image->pixels = (uint32_t*)malloc(pixel_bytes);

    if (image->pixels == NULL) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushstring(state, "out of memory");
        return 2;
    }

    result = f_read(&file, image->pixels, (UINT)pixel_bytes, &read_count);
    (void)f_close(&file);

    if (result != FR_OK || read_count != (UINT)pixel_bytes) {
        free(image->pixels);
        image->pixels = NULL;
        lua_pop(state, 1);
        lua_pushnil(state);
        lua_pushstring(state, "cannot read PKICN pixels");
        return 2;
    }

    return 1;
}

static int bitmap_load(lua_State* state) {
    const char* path = luaL_checkstring(state, 1);
    const char* extension = strrchr(path, '.');

    if (extension == NULL) {
        lua_pushnil(state);
        lua_pushstring(state, "image file has no extension");
        return 2;
    }

    if (
        strcmp(extension, ".pkicn") == 0 ||
        strcmp(extension, ".PKICN") == 0
    ) {
        return bitmap_load_pkicn(state);
    }

    lua_pushnil(state);
    lua_pushfstring(state, "unsupported image format: %s", extension);
    return 2;
}

static int bitmap_draw(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);
    int x = (int)luaL_checkinteger(state, 2);
    int y = (int)luaL_checkinteger(state, 3);
    int scale = (int)luaL_optinteger(state, 4, 1);
    uint16_t px;
    uint16_t py;

    if (image->pixels == NULL) {
        return luaL_error(state, "bitmap is freed");
    }

    if (scale < 1) {
        return luaL_error(state, "scale must be >= 1");
    }

    for (py = 0; py < image->height; py++) {
        for (px = 0; px < image->width; px++) {
            uint32_t color = image->pixels[(size_t)py * image->width + px];
            uint8_t alpha = (uint8_t)((color >> 24) & 0xFF);

            if (alpha == 0) {
                continue;
            }

            gfx_fill_rect(
                x + px * scale,
                y + py * scale,
                scale,
                scale,
                color
            );
        }
    }

    return 0;
}

static int bitmap_width(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);

    lua_pushinteger(state, image->width);
    return 1;
}

static int bitmap_height(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);

    lua_pushinteger(state, image->height);
    return 1;
}

static int bitmap_size(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);

    lua_pushinteger(state, image->width);
    lua_pushinteger(state, image->height);
    return 2;
}

static const luaL_Reg bitmap_methods[] = {
    { "draw", bitmap_draw },
    { "width", bitmap_width },
    { "height", bitmap_height },
    { "size", bitmap_size },
    { NULL, NULL }
};

static const luaL_Reg bitmap_module[] = {
    { "load", bitmap_load },
    { "draw", bitmap_draw },
    { "width", bitmap_width },
    { "height", bitmap_height },
    { "size", bitmap_size },
    { NULL, NULL }
};

int luaopen_bitmap(lua_State* state) {
    luaL_newmetatable(state, BITMAP_METATABLE);

    lua_pushcfunction(state, bitmap_gc);
    lua_setfield(state, -2, "__gc");

    lua_newtable(state);
    luaL_setfuncs(state, bitmap_methods, 0);
    lua_setfield(state, -2, "__index");

    lua_pop(state, 1);

    luaL_newlib(state, bitmap_module);
    return 1;
}
