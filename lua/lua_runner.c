#include <stddef.h>

#include "console.h"
#include "input.h"
#include "stdlib.h"
#include "lua_runner.h"
#include "program_runtime.h"

#include "ff.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "arch.h"

#if GFX_ENABLED
#include "lua_gfx.h"
#endif

#include "lua_input.h"
#include "lua_fs.h"

#define LUA_RUNNER_ATTR __attribute__((noinline, used, optimize("O0")))
#define LUA_RUNNER_CLOSE_HOOK_INSTRUCTIONS 1024
#define LUA_RUNNER_ALT_F4_EXIT_STATUS 0

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

static void lua_program_close_hook(
    lua_State* state,
    lua_Debug* debug
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
            arch_wait_for_event();
        }
    }

    lua_pushlightuserdata(session->state, session);
    (void)lua_error(session->state);

    for (;;) {
        arch_wait_for_event();
    }
}


static void lua_program_close_hook(
    lua_State* state,
    lua_Debug* debug
) {
    (void)state;
    (void)debug;

    input_update();

    if (input_take_global_close_request()) {
        program_exit(LUA_RUNNER_ALT_F4_EXIT_STATUS);
    }
}


static void neon_lua_set_args(
    lua_State* state,
    const char* path,
    int argc,
    char** argv
) {
    int index;

    lua_createtable(state, argc, 1);

    lua_pushstring(state, path);
    lua_rawseti(state, -2, 0);

    for (index = 0; index < argc; index++) {
        lua_pushstring(
            state,
            argv[index] != NULL ? argv[index] : ""
        );
        lua_rawseti(state, -2, index + 1);
    }

    lua_setglobal(state, "arg");
}


int lua_run_file_args(const char* path, int argc, char** argv) {
    LuaFileReader reader;
    LuaRunSession session;
    lua_State* state;
    int status;
    int exit_status;

    if (path == NULL || path[0] == '\0') {
        console_write("lua: no script path\n");
        return LUA_RUNNER_ERR_INVALID_PATH;
    }

    if (argc < 0 || (argc > 0 && argv == NULL)) {
        console_write("lua: invalid arguments\n");
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

    luaL_openlibs(state);

#if GFX_ENABLED
    luaL_requiref(state, "gfx", luaopen_gfx, 1);
    lua_pop(state, 1);
#endif

    luaL_requiref(state, "input", luaopen_input, 1);
    lua_pop(state, 1);

    luaL_requiref(state, "fs", luaopen_fs, 1);
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

    neon_lua_set_args(state, path, argc, argv);

    session.state = state;

    program_context_enter(
        &session.program,
        lua_program_exit_handler,
        &session
    );

    (void)input_take_global_close_request();

    lua_sethook(
        state,
        lua_program_close_hook,
        LUA_MASKCOUNT,
        LUA_RUNNER_CLOSE_HOOK_INSTRUCTIONS
    );

    console_suspend();

    for (int index = 0; index < argc; index++) {
        lua_pushstring(
            state,
            argv[index] != NULL ? argv[index] : ""
        );
    }

    status = lua_pcall(state, argc, LUA_MULTRET, 0);

    lua_sethook(state, NULL, 0, 0);

    console_resume();

    if (program_context_exit_requested(&session.program)) {
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

int lua_run_file(const char* path) {
    return lua_run_file_args(path, 0, NULL);
}
