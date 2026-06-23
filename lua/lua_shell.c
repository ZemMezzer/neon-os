#include "lua_shell.h"

#include <stddef.h>

#include "console.h"
#include "lua_runner.h"
#include "shell_commands.h"

#if PACKAGES_ENABLED
#include "lua_package_runner.h"
#endif

#define LUA_SHELL_PATH_MAX 512

static int lua_shell_strlen(const char* text) {
    int length = 0;

    while (text && text[length]) {
        length++;
    }

    return length;
}

static int lua_shell_lower(int value) {
    if (value >= 'A' && value <= 'Z') {
        return value - 'A' + 'a';
    }

    return value;
}

static int lua_shell_ends_with_lua(const char* text) {
    int length = lua_shell_strlen(text);

    return
        length >= 4 &&
        text[length - 4] == '.' &&
        lua_shell_lower(text[length - 3]) == 'l' &&
        lua_shell_lower(text[length - 2]) == 'u' &&
        lua_shell_lower(text[length - 1]) == 'a';
}

static void lua_shell_copy(
    char* destination,
    int capacity,
    const char* source
) {
    int index = 0;

    if (!destination || capacity <= 0) {
        return;
    }

    if (!source) {
        destination[0] = 0;
        return;
    }

    while (source[index] && index + 1 < capacity) {
        destination[index] = source[index];
        index++;
    }

    destination[index] = 0;
}

static int lua_shell_command_name_is_safe(const char* name) {
    int index = 0;

    if (!name || name[0] == 0) {
        return 0;
    }

    while (name[index]) {
        char character = name[index];

        if (
            character == '/' ||
            character == '\\' ||
            character == ':' ||
            character == ' ' ||
            character == '\t'
        ) {
            return 0;
        }

        index++;
    }

    return 1;
}

static int lua_shell_make_program_filename(
    const char* command,
    char* output,
    int output_capacity
) {
    int length;

    if (
        !command ||
        !output ||
        output_capacity <= 0 ||
        !lua_shell_command_name_is_safe(command)
    ) {
        return -1;
    }

    lua_shell_copy(output, output_capacity, command);
    length = lua_shell_strlen(output);

    if (command[length] != 0) {
        return -1;
    }

    if (!lua_shell_ends_with_lua(output)) {
        if (length + 4 + 1 > output_capacity) {
            return -1;
        }

        output[length] = '.';
        output[length + 1] = 'l';
        output[length + 2] = 'u';
        output[length + 3] = 'a';
        output[length + 4] = 0;
    }

    return 0;
}

static int lua_shell_run(
    const char* path,
    int argument_count,
    char** arguments
) {
    int status = lua_run_file_args(path, argument_count, arguments);

    return status < 0 ? 1 : status;
}

static int cmd_lua(int argc, char** argv) {
    char path[LUA_SHELL_PATH_MAX];

    if (argc < 2) {
        console_write("usage: lua <script.lua> [arguments...]\n");
        return 1;
    }

    if (!lua_shell_ends_with_lua(argv[1])) {
        console_write("lua: script must end with .lua\n");
        return 1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        console_write("lua: path too long\n");
        return 1;
    }

    return lua_shell_run(path, argc - 2, argv + 2);
}

static int lua_shell_command_fallback(
    int argc,
    char** argv,
    int* out_status
) {
    char filename[LUA_SHELL_PATH_MAX];
    char path[LUA_SHELL_PATH_MAX];
    int found;

    if (argc <= 0 || !argv || !out_status) {
        return 0;
    }

    if (
        lua_shell_make_program_filename(
            argv[0],
            filename,
            sizeof(filename)
        ) != 0
    ) {
        return 0;
    }

    found = shell_find_command_file(filename, path, sizeof(path));

    if (found != 1) {
        return 0;
    }

    *out_status = lua_shell_run(path, argc - 1, argv + 1);
    return 1;
}

void lua_shell_register_commands(void) {
    (void)shell_register_command(
        "lua",
        "run a Lua script",
        cmd_lua
    );

    (void)shell_set_command_fallback(lua_shell_command_fallback);

#if PACKAGES_ENABLED
    if (lua_package_runner_register() != 0) {
        console_write("lua: cannot register package runner\n");
    }
#endif
}
