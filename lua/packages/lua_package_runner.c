#include "lua_package_runner.h"

#include <stddef.h>

#include "lua_runner.h"
#include "neon_fs.h"
#include "package_runner.h"

#define LUA_PACKAGE_ENTRY "main.lua"

static int lua_package_runner_registered = 0;

static int lua_package_join_entry(
    const char* package_path,
    char* output,
    int output_size
) {
    int position = 0;
    int index = 0;

    if (
        package_path == NULL ||
        package_path[0] == '\0' ||
        output == NULL ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = '\0';

    while (package_path[index] != '\0') {
        if (position + 1 >= output_size) {
            return -1;
        }

        output[position++] = package_path[index++];
    }

    if (
        position == 0 ||
        output[position - 1] != '/'
    ) {
        if (position + 1 >= output_size) {
            return -1;
        }

        output[position++] = '/';
    }

    index = 0;

    while (LUA_PACKAGE_ENTRY[index] != '\0') {
        if (position + 1 >= output_size) {
            return -1;
        }

        output[position++] = LUA_PACKAGE_ENTRY[index++];
    }

    output[position] = '\0';
    return 0;
}

static PackageRunnerResult lua_package_runner_open(
    const char* package_path,
    int argc,
    char** argv,
    int* out_status
) {
    char entry_path[NEON_FS_PATH_MAX];
    int status;

    if (out_status == NULL) {
        return PACKAGE_RUNNER_ERROR;
    }

    if (
        lua_package_join_entry(
            package_path,
            entry_path,
            sizeof(entry_path)
        ) != 0
    ) {
        return PACKAGE_RUNNER_ERROR;
    }

    if (!file_exists(entry_path)) {
        return PACKAGE_RUNNER_NOT_APPLICABLE;
    }

    status = lua_run_file_args(entry_path, argc, argv);

    *out_status = status < 0 ? 1 : status;
    return PACKAGE_RUNNER_STARTED;
}

int lua_package_runner_register(void) {
    int result;

    if (lua_package_runner_registered) {
        return 0;
    }

    result = package_runner_register(
        "lua",
        lua_package_runner_open
    );

    if (result != 0) {
        return -1;
    }

    lua_package_runner_registered = 1;
    return 0;
}
