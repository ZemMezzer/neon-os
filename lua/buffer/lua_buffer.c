#include "lua_buffer.h"

#include <stddef.h>

#include "lauxlib.h"
#include "shared_buffer.h"

static int lua_buffer_return_error(
    lua_State* state,
    SharedBufferStatus status
) {
    lua_pushnil(state);
    lua_pushstring(state, shared_buffer_status_string(status));
    return 2;
}

static int lua_buffer_set(lua_State* state) {
    const char* key = luaL_checkstring(state, 1);
    const char* value = luaL_checkstring(state, 2);
    SharedBufferStatus status = shared_buffer_set(key, value);

    if (status != SHARED_BUFFER_OK) {
        return lua_buffer_return_error(state, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}

static int lua_buffer_get(lua_State* state) {
    const char* key = luaL_checkstring(state, 1);
    char value[SHARED_BUFFER_VALUE_MAX];
    SharedBufferStatus status = shared_buffer_get(
        key,
        value,
        sizeof(value)
    );

    if (status == SHARED_BUFFER_ERR_NOT_FOUND) {
        lua_pushnil(state);
        return 1;
    }

    if (status != SHARED_BUFFER_OK) {
        return lua_buffer_return_error(state, status);
    }

    lua_pushstring(state, value);
    return 1;
}

static int lua_buffer_take(lua_State* state) {
    const char* key = luaL_checkstring(state, 1);
    char value[SHARED_BUFFER_VALUE_MAX];
    SharedBufferStatus status = shared_buffer_take(
        key,
        value,
        sizeof(value)
    );

    if (status == SHARED_BUFFER_ERR_NOT_FOUND) {
        lua_pushnil(state);
        return 1;
    }

    if (status != SHARED_BUFFER_OK) {
        return lua_buffer_return_error(state, status);
    }

    lua_pushstring(state, value);
    return 1;
}

static int lua_buffer_clear(lua_State* state) {
    const char* key = luaL_checkstring(state, 1);
    SharedBufferStatus status = shared_buffer_clear(key);

    if (status == SHARED_BUFFER_ERR_NOT_FOUND) {
        lua_pushboolean(state, 0);
        return 1;
    }

    if (status != SHARED_BUFFER_OK) {
        return lua_buffer_return_error(state, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}

static int lua_buffer_exists(lua_State* state) {
    const char* key = luaL_checkstring(state, 1);

    lua_pushboolean(state, shared_buffer_exists(key));
    return 1;
}

static int lua_buffer_clear_all(lua_State* state) {
    (void)state;
    shared_buffer_clear_all();
    return 0;
}


static int lua_buffer_clipboard_set(lua_State* state) {
    const char* value = luaL_checkstring(state, 1);
    SharedBufferStatus status = shared_buffer_clipboard_set(value);

    if (status != SHARED_BUFFER_OK) {
        return lua_buffer_return_error(state, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}

static int lua_buffer_clipboard_get(lua_State* state) {
    char value[SHARED_BUFFER_CLIPBOARD_VALUE_MAX];
    SharedBufferStatus status = shared_buffer_clipboard_get(
        value,
        sizeof(value)
    );

    if (status == SHARED_BUFFER_ERR_NOT_FOUND) {
        lua_pushnil(state);
        return 1;
    }

    if (status != SHARED_BUFFER_OK) {
        return lua_buffer_return_error(state, status);
    }

    lua_pushstring(state, value);
    return 1;
}

static int lua_buffer_clipboard_clear(lua_State* state) {
    SharedBufferStatus status = shared_buffer_clipboard_clear();

    if (status == SHARED_BUFFER_ERR_NOT_FOUND) {
        lua_pushboolean(state, 0);
        return 1;
    }

    if (status != SHARED_BUFFER_OK) {
        return lua_buffer_return_error(state, status);
    }

    lua_pushboolean(state, 1);
    return 1;
}

static const luaL_Reg lua_buffer_library[] = {
    { "set", lua_buffer_set },
    { "get", lua_buffer_get },
    { "take", lua_buffer_take },
    { "clear", lua_buffer_clear },
    { "exists", lua_buffer_exists },
    { "clear_all", lua_buffer_clear_all },
    { "clipboard_set", lua_buffer_clipboard_set },
    { "clipboard_get", lua_buffer_clipboard_get },
    { "clipboard_clear", lua_buffer_clipboard_clear },
    { NULL, NULL }
};

int luaopen_buffer(lua_State* state) {
    luaL_newlib(state, lua_buffer_library);
    return 1;
}
