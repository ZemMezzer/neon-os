#include "lua_shell_api.h"

#include <stddef.h>
#include <stdlib.h>

#include "lauxlib.h"

#include "console.h"
#include "shell_commands.h"

#define LUA_SHELL_CAPTURE_CAPACITY 16384

typedef int (*LuaShellActionFn)(const char* argument);

typedef struct LuaShellCapture {
    char* buffer;
    size_t length;
    size_t capacity;
    int truncated;
} LuaShellCapture;


static void lua_shell_capture_character(
    char character,
    void* userdata
) {
    LuaShellCapture* capture = (LuaShellCapture*)userdata;

    if (capture == NULL || capture->buffer == NULL) {
        return;
    }

    if (capture->length >= capture->capacity) {
        capture->truncated = 1;
        return;
    }

    capture->buffer[capture->length] = character;
    capture->length++;
}


static void lua_shell_capture_append_marker(LuaShellCapture* capture) {
    static const char marker[] = "\n[output truncated]\n";
    size_t marker_length = sizeof(marker) - 1;
    size_t start;
    size_t index;

    if (
        capture == NULL ||
        capture->buffer == NULL ||
        !capture->truncated ||
        capture->capacity == 0
    ) {
        return;
    }

    if (marker_length > capture->capacity) {
        marker_length = capture->capacity;
    }

    if (capture->length + marker_length <= capture->capacity) {
        start = capture->length;
    } else {
        start = capture->capacity - marker_length;
    }

    for (index = 0; index < marker_length; index++) {
        capture->buffer[start + index] = marker[index];
    }

    capture->length = start + marker_length;
}


static int lua_shell_call_with_capture(
    lua_State* state,
    LuaShellActionFn action
) {
    const char* argument;
    char* output;
    LuaShellCapture capture;
    ConsoleOutputTarget previous_target;
    int status;

    argument = luaL_checkstring(state, 1);

    output = (char*)malloc(LUA_SHELL_CAPTURE_CAPACITY);

    if (output == NULL) {
        return luaL_error(state, "shell: cannot allocate output capture buffer");
    }

    capture.buffer = output;
    capture.length = 0;
    capture.capacity = LUA_SHELL_CAPTURE_CAPACITY;
    capture.truncated = 0;

    previous_target = console_set_output_callback(
        lua_shell_capture_character,
        &capture
    );

    status = action(argument);

    (void)console_set_output_callback(
        previous_target.callback,
        previous_target.userdata
    );

    lua_shell_capture_append_marker(&capture);

    lua_pushinteger(state, (lua_Integer)status);
    lua_pushlstring(state, output, capture.length);

    free(output);
    return 2;
}

static int lua_shell_exec(lua_State* state) {
    const char* command = luaL_checkstring(state, 1);
    int status = shell_commands_execute(command);

    lua_pushinteger(state, (lua_Integer)status);
    return 1;
}

static int lua_shell_exec_capture(lua_State* state) {
    return lua_shell_call_with_capture(
        state,
        shell_commands_execute
    );
}

static int lua_shell_run_script(lua_State* state) {
    const char* path = luaL_checkstring(state, 1);
    int status = shell_run_script(path);

    lua_pushinteger(state, (lua_Integer)status);
    return 1;
}

static int lua_shell_run_script_capture(lua_State* state) {
    return lua_shell_call_with_capture(
        state,
        shell_run_script
    );
}


static const luaL_Reg lua_shell_library[] = {
    { "exec",               lua_shell_exec },
    { "exec_capture",       lua_shell_exec_capture },
    { "run_script",         lua_shell_run_script },
    { "run_script_capture", lua_shell_run_script_capture },
    { NULL,                  NULL }
};


int luaopen_shell(lua_State* state) {
    luaL_newlib(state, lua_shell_library);
    return 1;
}
