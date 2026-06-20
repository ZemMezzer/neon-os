#include <stddef.h>

#include "console.h"
#include "stdlib.h"
#include "lua_runner.h"

#include "ff.h"
#include "lua.h"

#define LUA_RUNNER_ATTR __attribute__((noinline, used, optimize("O0")))

typedef struct LuaFileReader {
    FIL file;
    FRESULT result;
    char buffer[512];
    int opened;
} LuaFileReader;

static void* neon_lua_allocator(
    void* user_data,
    void* pointer,
    size_t old_size,
    size_t new_size
) LUA_RUNNER_ATTR;

static const char* neon_lua_file_reader(
    lua_State* state,
    void* user_data,
    size_t* size
) LUA_RUNNER_ATTR;

static void neon_lua_print_error(
    lua_State* state,
    const char* stage
) LUA_RUNNER_ATTR;

static void neon_lua_write_integer(lua_Integer value) LUA_RUNNER_ATTR;
static void neon_lua_write_value(lua_State* state, int index) LUA_RUNNER_ATTR;
static int neon_lua_print(lua_State* state) LUA_RUNNER_ATTR;

static void* neon_lua_allocator(
    void* user_data,
    void* pointer,
    size_t old_size,
    size_t new_size
) {
    (void)user_data;
    (void)old_size;

    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    return realloc(pointer, new_size);
}

static const char* neon_lua_file_reader(
    lua_State* state,
    void* user_data,
    size_t* size
) {
    LuaFileReader* reader;
    UINT bytes_read;

    (void)state;

    reader = (LuaFileReader*)user_data;

    if (reader == NULL || size == NULL || !reader->opened) {
        if (size != NULL) {
            *size = 0;
        }

        return NULL;
    }

    bytes_read = 0;

    reader->result = f_read(
        &reader->file,
        reader->buffer,
        (UINT)sizeof(reader->buffer),
        &bytes_read
    );

    if (reader->result != FR_OK || bytes_read == 0) {
        *size = 0;
        return NULL;
    }

    *size = (size_t)bytes_read;

    return reader->buffer;
}

static void neon_lua_write_integer(lua_Integer value) {
    char buffer[32];
    size_t position;
    lua_Unsigned number;
    int negative;

    position = sizeof(buffer) - 1;
    buffer[position] = '\0';

    negative = (value < 0);

    if (negative) {
        number = (lua_Unsigned)(-(value + 1));
        number++;
    }
    else {
        number = (lua_Unsigned)value;
    }

    do {
        buffer[--position] = (char)('0' + (number % 10));
        number /= 10;
    } while (number != 0);

    if (negative) {
        buffer[--position] = '-';
    }

    console_write(&buffer[position]);
}

static void neon_lua_write_value(lua_State* state, int index) {
    int type;
    const char* text;

    type = lua_type(state, index);

    switch (type) {
        case LUA_TNIL:
            console_write("nil");
            break;

        case LUA_TBOOLEAN:
            console_write(lua_toboolean(state, index) ? "true" : "false");
            break;

        case LUA_TNUMBER:
            if (lua_isinteger(state, index)) {
                neon_lua_write_integer(lua_tointeger(state, index));
            }
            else {
                console_write("<float>");
            }
            break;

        case LUA_TSTRING:
            text = lua_tolstring(state, index, NULL);
            console_write(text != NULL ? text : "<string error>");
            break;

        default:
            console_write("<");
            console_write(lua_typename(state, type));
            console_write(">");
            break;
    }
}

static int neon_lua_print(lua_State* state) {
    int count;
    int index;

    count = lua_gettop(state);

    for (index = 1; index <= count; index++) {
        neon_lua_write_value(state, index);

        if (index != count) {
            console_write("\t");
        }
    }

    console_write("\n");

    return 0;
}

static void neon_lua_print_error(
    lua_State* state,
    const char* stage
) {
    const char* message;

    console_write("lua error");

    if (stage != NULL) {
        console_write(" [");
        console_write(stage);
        console_write("]");
    }

    console_write(": ");

    message = lua_tolstring(state, -1, NULL);

    if (message != NULL) {
        console_write(message);
    }
    else {
        console_write("(no error message)");
    }

    console_write("\n");
}

int lua_run_file(const char* path) {
    LuaFileReader reader;
    lua_State* state;
    int status;

    if (path == NULL || path[0] == '\0') {
        console_write("lua: no script path\n");
        return -1;
    }

    reader.opened = 0;
    reader.result = f_open(&reader.file, path, FA_READ);

    if (reader.result != FR_OK) {
        console_write("lua: cannot open ");
        console_write(path);
        console_write("\n");
        return -3;
    }

    reader.opened = 1;

    state = lua_newstate(
        neon_lua_allocator,
        NULL,
        0x4E454F4EU
    );

    if (state == NULL) {
        f_close(&reader.file);
        console_write("lua: state creation failed\n");
        return -2;
    }

    lua_pushcfunction(state, neon_lua_print);
    lua_setglobal(state, "print");

    status = lua_load(
        state,
        neon_lua_file_reader,
        &reader,
        path,
        "t"
    );

    f_close(&reader.file);
    reader.opened = 0;

    if (reader.result != FR_OK) {
        console_write("lua: file read error\n");
        lua_close(state);
        return -4;
    }

    if (status != LUA_OK) {
        neon_lua_print_error(state, "load");
        lua_close(state);
        return -5;
    }

    status = lua_pcall(state, 0, LUA_MULTRET, 0);

    if (status != LUA_OK) {
        neon_lua_print_error(state, "runtime");
        lua_close(state);
        return -6;
    }

    lua_settop(state, 0);
    lua_close(state);

    return 0;
}
