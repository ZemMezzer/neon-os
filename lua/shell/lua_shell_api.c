#include "lua_shell_api.h"

#include "lauxlib.h"

#include "shell_commands.h"

static int lua_shell_exec(lua_State* L) {
    const char* command = luaL_checkstring(L, 1);
    int status = shell_commands_execute(command);

    lua_pushinteger(L, (lua_Integer)status);
    return 1;
}

static int lua_shell_run_script(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int status = shell_run_script(path);

    lua_pushinteger(L, (lua_Integer)status);
    return 1;
}

static const luaL_Reg lua_shell_library[] = {
    { "exec",       lua_shell_exec },
    { "run_script", lua_shell_run_script },
    { NULL,         NULL }
};

int luaopen_shell(lua_State* L) {
    luaL_newlib(L, lua_shell_library);
    return 1;
}
