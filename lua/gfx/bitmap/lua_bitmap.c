#include "lua_bitmap.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "ff.h"
#include "gfx.h"

#define BITMAP_METATABLE "NeonOS.bitmap"

#define PKICN_HEADER_SIZE 12u
#define PKICN_VERSION 1u
#define PKICN_FLAGS_ARGB8888 1u

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


static void bitmap_write_u16_le(unsigned char* data, uint16_t value) {
    data[0] = (unsigned char)(value & 0xFFu);
    data[1] = (unsigned char)((value >> 8) & 0xFFu);
}


static void bitmap_write_u32_le(unsigned char* data, uint32_t value) {
    data[0] = (unsigned char)(value & 0xFFu);
    data[1] = (unsigned char)((value >> 8) & 0xFFu);
    data[2] = (unsigned char)((value >> 16) & 0xFFu);
    data[3] = (unsigned char)((value >> 24) & 0xFFu);
}


static BitmapImage* bitmap_check(lua_State* state, int index) {
    return (BitmapImage*)luaL_checkudata(state, index, BITMAP_METATABLE);
}


static int bitmap_is_alive(const BitmapImage* image) {
    return image != NULL && image->pixels != NULL;
}


static int bitmap_read_dimensions(
    lua_State* state,
    int width_index,
    int height_index,
    uint16_t* out_width,
    uint16_t* out_height,
    size_t* out_pixel_count
) {
    lua_Integer width = luaL_checkinteger(state, width_index);
    lua_Integer height = luaL_checkinteger(state, height_index);
    size_t pixel_count;

    if (width < 1 || width > (lua_Integer)UINT16_MAX) {
        luaL_error(state, "width must be in range 1..65535");
        return 0;
    }

    if (height < 1 || height > (lua_Integer)UINT16_MAX) {
        luaL_error(state, "height must be in range 1..65535");
        return 0;
    }

    pixel_count = (size_t)width * (size_t)height;

    if (pixel_count > ((size_t)-1) / sizeof(uint32_t)) {
        luaL_error(state, "bitmap is too large");
        return 0;
    }

    *out_width = (uint16_t)width;
    *out_height = (uint16_t)height;
    *out_pixel_count = pixel_count;
    return 1;
}


static BitmapImage* bitmap_push_image(
    lua_State* state,
    uint16_t width,
    uint16_t height,
    size_t pixel_count
) {
    BitmapImage* image = (BitmapImage*)lua_newuserdatauv(
        state,
        sizeof(BitmapImage),
        0
    );

    image->width = width;
    image->height = height;
    image->pixels = NULL;

    luaL_getmetatable(state, BITMAP_METATABLE);
    lua_setmetatable(state, -2);

    image->pixels = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));

    if (image->pixels == NULL) {
        lua_pop(state, 1);
        return NULL;
    }

    return image;
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
    unsigned char header[PKICN_HEADER_SIZE];
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

    if (version != PKICN_VERSION) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushfstring(
            state,
            "unsupported PKICN version: %d",
            (int)version
        );
        return 2;
    }

    if (width == 0 || height == 0) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushstring(state, "invalid image size");
        return 2;
    }

    if (flags != PKICN_FLAGS_ARGB8888) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushfstring(
            state,
            "unsupported PKICN flags: %d",
            (int)flags
        );
        return 2;
    }

    pixel_count = (size_t)width * (size_t)height;

    if (pixel_count > ((size_t)-1) / sizeof(uint32_t)) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushstring(state, "image is too large");
        return 2;
    }

    pixel_bytes = pixel_count * sizeof(uint32_t);

    if (pixel_bytes > (size_t)((UINT)-1)) {
        (void)f_close(&file);
        lua_pushnil(state);
        lua_pushstring(state, "image is too large for the file reader");
        return 2;
    }

    image = bitmap_push_image(state, width, height, pixel_count);

    if (image == NULL) {
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


static int bitmap_new(lua_State* state) {
    uint16_t width;
    uint16_t height;
    size_t pixel_count;
    lua_Integer fill_color = luaL_optinteger(state, 3, 0);
    BitmapImage* image;
    size_t index;

    if (!bitmap_read_dimensions(
        state,
        1,
        2,
        &width,
        &height,
        &pixel_count
    )) {
        return 0;
    }

    if (fill_color < 0 || fill_color > (lua_Integer)UINT32_MAX) {
        return luaL_error(
            state,
            "fill color must be in range 0x00000000..0xFFFFFFFF"
        );
    }

    image = bitmap_push_image(state, width, height, pixel_count);

    if (image == NULL) {
        lua_pushnil(state);
        lua_pushstring(state, "out of memory");
        return 2;
    }

    for (index = 0; index < pixel_count; index++) {
        image->pixels[index] = (uint32_t)fill_color;
    }

    return 1;
}


static int bitmap_draw(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);
    int x = (int)luaL_checkinteger(state, 2);
    int y = (int)luaL_checkinteger(state, 3);
    int scale = (int)luaL_optinteger(state, 4, 1);
    uint16_t px;
    uint16_t py;

    if (!bitmap_is_alive(image)) {
        return luaL_error(state, "bitmap is freed");
    }

    if (scale < 1) {
        return luaL_error(state, "scale must be >= 1");
    }

    for (py = 0; py < image->height; py++) {
        for (px = 0; px < image->width; px++) {
            uint32_t color = image->pixels[(size_t)py * image->width + px];
            uint8_t alpha = (uint8_t)((color >> 24) & 0xFFu);

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


static int bitmap_get_pixel(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);
    lua_Integer x = luaL_checkinteger(state, 2);
    lua_Integer y = luaL_checkinteger(state, 3);
    size_t index;

    if (!bitmap_is_alive(image)) {
        return luaL_error(state, "bitmap is freed");
    }

    /* Pixel coordinates are zero-based: 0 <= x < width, 0 <= y < height. */
    if (
        x < 0 ||
        y < 0 ||
        x >= (lua_Integer)image->width ||
        y >= (lua_Integer)image->height
    ) {
        lua_pushnil(state);
        return 1;
    }

    index = (size_t)y * (size_t)image->width + (size_t)x;
    lua_pushinteger(state, (lua_Integer)image->pixels[index]);
    return 1;
}


static int bitmap_set_pixel(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);
    lua_Integer x = luaL_checkinteger(state, 2);
    lua_Integer y = luaL_checkinteger(state, 3);
    lua_Integer color = luaL_checkinteger(state, 4);
    size_t index;

    if (!bitmap_is_alive(image)) {
        return luaL_error(state, "bitmap is freed");
    }

    if (color < 0 || color > (lua_Integer)UINT32_MAX) {
        return luaL_error(
            state,
            "color must be in range 0x00000000..0xFFFFFFFF"
        );
    }

    /* Pixel coordinates are zero-based: 0 <= x < width, 0 <= y < height. */
    if (
        x < 0 ||
        y < 0 ||
        x >= (lua_Integer)image->width ||
        y >= (lua_Integer)image->height
    ) {
        lua_pushboolean(state, 0);
        return 1;
    }

    index = (size_t)y * (size_t)image->width + (size_t)x;
    image->pixels[index] = (uint32_t)color;

    lua_pushboolean(state, 1);
    return 1;
}


static int bitmap_to_bytes(lua_State* state) {
    BitmapImage* image = bitmap_check(state, 1);
    size_t pixel_count;
    size_t pixel_bytes;
    size_t total_bytes;
    luaL_Buffer buffer;
    unsigned char* output;
    size_t index;

    if (!bitmap_is_alive(image)) {
        return luaL_error(state, "bitmap is freed");
    }

    pixel_count = (size_t)image->width * (size_t)image->height;
    pixel_bytes = pixel_count * sizeof(uint32_t);

    if (pixel_bytes > ((size_t)-1) - PKICN_HEADER_SIZE) {
        return luaL_error(state, "bitmap is too large to encode");
    }

    total_bytes = PKICN_HEADER_SIZE + pixel_bytes;
    output = (unsigned char*)luaL_buffinitsize(state, &buffer, total_bytes);

    output[0] = 'P';
    output[1] = 'K';
    output[2] = 'I';
    output[3] = 'C';
    bitmap_write_u16_le(output + 4, PKICN_VERSION);
    bitmap_write_u16_le(output + 6, image->width);
    bitmap_write_u16_le(output + 8, image->height);
    bitmap_write_u16_le(output + 10, PKICN_FLAGS_ARGB8888);

    for (index = 0; index < pixel_count; index++) {
        bitmap_write_u32_le(
            output + PKICN_HEADER_SIZE + index * sizeof(uint32_t),
            image->pixels[index]
        );
    }

    luaL_pushresultsize(&buffer, total_bytes);
    return 1;
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
    { "getPixel", bitmap_get_pixel },
    { "setPixel", bitmap_set_pixel },
    { "toBytes", bitmap_to_bytes },
    { "width", bitmap_width },
    { "height", bitmap_height },
    { "size", bitmap_size },
    { NULL, NULL }
};


static const luaL_Reg bitmap_module[] = {
    { "load", bitmap_load },
    { "new", bitmap_new },
    { "draw", bitmap_draw },
    { "getPixel", bitmap_get_pixel },
    { "setPixel", bitmap_set_pixel },
    { "toBytes", bitmap_to_bytes },
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
