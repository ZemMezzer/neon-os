#include "lua_shell.h"

#include "console.h"
#include "lua_runner.h"
#include "shell_commands.h"

#define LUA_SHELL_PATH_MAX 512


static int cmd_lua(int argc, char** argv) {
    char path[LUA_SHELL_PATH_MAX];
    int status;

    if (argc < 2) {
        console_write("usage: lua <script>\n");
        return 1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        console_write("lua: path too long\n");
        return 1;
    }

    status = lua_run_file(path);

    /*
        Negative values are NeonOS-internal loader/runtime failures.
        Shell and system() expose conventional nonzero program status.
    */
    if (status < 0) {
        return 1;
    }

    return status;
}


void lua_shell_register_commands(void) {
    (void)shell_register_command(
        "lua",
        "run a Lua script",
        cmd_lua
    );
}
