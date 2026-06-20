#include "shell_commands.h"
#include "console.h"
#include "lua_runner.h"

#define SHELL_PATH_MAX 512

static int cmd_lua(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    int result;

    if (argc < 2) {
        console_write("Error: usage: lua <file>\n");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        console_write("Error: path too long\n");
        return -1;
    }

    result = lua_run_file(path);

    if (result != 0) {
        return -1;
    }

    return 0;
}

void lua_shell_register_commands(void) {
    shell_register_command(
        "lua",
        "run Lua script",
        cmd_lua
    );
}