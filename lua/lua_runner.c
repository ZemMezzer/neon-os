#include <stddef.h>

#include "console.h"
#include "stdlib.h"
#include "lua_runner.h"
#include "lua_gfx.h"
#include "lua_input.h"
#include "program_runtime.h"

#include "ff.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define LUA_RUNNER_ATTR __attribute__((noinline, used, optimize("O0")))

typedef struct LuaFileReader {
    FIL file;
    FRESULT result;
    char buffer[512];
    int opened;
} LuaFileReader;

typedef struct LuaRunSession {
    ProgramContext program;
    lua_State* state;
} LuaRunSession;

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

static void lua_program_exit_handler(
    ProgramContext* program,
    int status
) LUA_RUNNER_ATTR;


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


static void neon_lua_print_error(
    lua_State* state,
    const char* stage
) {
    const char* message;

    console_write("lua ");
    console_write(stage);
    console_write(": ");

    message = lua_tostring(state, -1);

    if (message != NULL) {
        console_write(message);
    } else {
        console_write("(no error message)");
    }

    console_write("\n");
    lua_pop(state, 1);
}


static void lua_program_exit_handler(
    ProgramContext* program,
    int status
) {
    LuaRunSession* session;

    (void)status;

    session = (LuaRunSession*)program_context_userdata(program);

    if (session == NULL || session->state == NULL) {
        for (;;) {
            asm volatile("wfe");
        }
    }

    /*
        Do not jump across Lua frames directly. This error reaches the
        lua_pcall() below through Lua's own protected-error mechanism.
    */
    lua_pushlightuserdata(session->state, session);
    (void)lua_error(session->state);

    for (;;) {
        asm volatile("wfe");
    }
}


int lua_run_file(const char* path) {
    LuaFileReader reader;
    LuaRunSession session;
    lua_State* state;
    int status;
    int exit_status;

    if (path == NULL || path[0] == '\0') {
        console_write("lua: no script path\n");
        return LUA_RUNNER_ERR_INVALID_PATH;
    }

    reader.opened = 0;
    reader.result = f_open(&reader.file, path, FA_READ);

    if (reader.result != FR_OK) {
        console_write("lua: cannot open ");
        console_write(path);
        console_write("\n");
        return LUA_RUNNER_ERR_OPEN_FILE;
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
        return LUA_RUNNER_ERR_STATE_CREATE;
    }

    /*
        Standard Lua libraries call ordinary C functions supplied by
        NeonOS libc and the generic program runtime.
    */
    luaL_openlibs(state);

    /*
        Register the built-in framebuffer module. luaL_requiref also places
        it into package.loaded, so Lua scripts can simply use:
            local gfx = require("gfx")
    */
    luaL_requiref(state, "gfx", luaopen_gfx, 1);
    lua_pop(state, 1);

    /*
        Register keyboard polling for Lua programs:
            local input = require("input")
            if input.any_pressed() then ... end
    */
    luaL_requiref(state, "input", luaopen_input, 1);
    lua_pop(state, 1);

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
        return LUA_RUNNER_ERR_READ_FILE;
    }

    if (status != LUA_OK) {
        neon_lua_print_error(state, "load error");
        lua_close(state);
        return LUA_RUNNER_ERR_LOAD;
    }

    session.state = state;

    program_context_enter(
        &session.program,
        lua_program_exit_handler,
        &session
    );

    /*
        The Lua application owns the framebuffer for the duration of pcall.
        The console is blanked and made draw-inactive, so old prompts, the
        blinking cursor, and Lua print output cannot overwrite GFX pixels.
    */
    console_suspend();

    status = lua_pcall(state, 0, LUA_MULTRET, 0);

    /*
        Return to a clean terminal before reporting a Lua error or allowing
        the shell to draw its next prompt.
    */
    console_resume();

    if (program_context_exit_requested(&session.program)) {
        /*
            exit() raised a Lua error intentionally. Do not print that
            internal marker; close the finished state and return its status.
        */
        exit_status = program_context_exit_status(&session.program);

        lua_settop(state, 0);
        program_context_leave(&session.program);
        lua_close(state);

        return exit_status;
    }

    program_context_leave(&session.program);

    if (status != LUA_OK) {
        neon_lua_print_error(state, "runtime error");
        lua_close(state);
        return LUA_RUNNER_ERR_RUNTIME;
    }

    lua_settop(state, 0);
    lua_close(state);

    return 0;
}
