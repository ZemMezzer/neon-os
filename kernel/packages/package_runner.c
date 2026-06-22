#include "package_runner.h"

#include <stddef.h>

#include "neon_fs.h"
#include "package_manager.h"
#include "shell_commands.h"

typedef struct PackageRunnerRecord {
    const char* name;
    PackageRunnerFn fn;
} PackageRunnerRecord;

static PackageRunnerRecord package_runners[PACKAGE_RUNNER_MAX];
static int package_runner_count = 0;

static int package_string_equal(const char* left, const char* right) {
    int index = 0;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }

        index++;
    }

    return left[index] == right[index];
}

static int package_runner_name_is_valid(const char* name) {
    int index = 0;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    while (name[index] != '\0') {
        char character = name[index];

        if (
            character == '/' ||
            character == '\\' ||
            character == ':' ||
            character == ' ' ||
            character == '\t' ||
            character == '\r' ||
            character == '\n'
        ) {
            return 0;
        }

        index++;
    }

    return 1;
}

int package_runner_is_package_directory(const char* absolute_path) {
    int direct_child;

    direct_child = shell_is_direct_child_of_path(absolute_path);

    if (direct_child != 1) {
        return direct_child;
    }

    return package_is_package_directory(absolute_path) ? 1 : 0;
}

int package_runner_register(
    const char* name,
    PackageRunnerFn fn
) {
    int index;

    if (!package_runner_name_is_valid(name) || fn == NULL) {
        return -1;
    }

    for (index = 0; index < package_runner_count; index++) {
        if (package_string_equal(package_runners[index].name, name)) {
            return package_runners[index].fn == fn ? 0 : -1;
        }
    }

    if (package_runner_count >= PACKAGE_RUNNER_MAX) {
        return -1;
    }

    package_runners[package_runner_count].name = name;
    package_runners[package_runner_count].fn = fn;
    package_runner_count++;

    return 0;
}

PackageOpenResult package_runner_open(
    const char* package_path,
    int argc,
    char** argv,
    int* out_status
) {
    char normalized_path[NEON_FS_PATH_MAX];
    char previous_directory[NEON_FS_PATH_MAX];
    int index;
    int previous_directory_saved = 0;
    int directory_changed = 0;
    PackageOpenResult result = PACKAGE_OPEN_NO_RUNNER;

    if (out_status == NULL || package_path == NULL || package_path[0] == '\0') {
        return PACKAGE_OPEN_ERROR;
    }

    *out_status = 1;

    if (
        shell_resolve_path(
            package_path,
            normalized_path,
            sizeof(normalized_path)
        ) != 0
    ) {
        return PACKAGE_OPEN_ERROR;
    }

    {
        int package_directory = package_runner_is_package_directory(
            normalized_path
        );

        if (package_directory < 0) {
            return PACKAGE_OPEN_ERROR;
        }

        if (package_directory == 0) {
            return PACKAGE_OPEN_NOT_PACKAGE;
        }
    }

    if (
        shell_get_current_directory(
            previous_directory,
            sizeof(previous_directory)
        ) != 0
    ) {
        return PACKAGE_OPEN_ERROR;
    }

    previous_directory_saved = 1;

    if (shell_set_current_directory(normalized_path) != 0) {
        return PACKAGE_OPEN_ERROR;
    }

    directory_changed = 1;

    for (index = 0; index < package_runner_count; index++) {
        PackageRunnerResult runner_result;

        runner_result = package_runners[index].fn(
            normalized_path,
            argc,
            argv,
            out_status
        );

        if (runner_result == PACKAGE_RUNNER_STARTED) {
            result = PACKAGE_OPEN_STARTED;
            break;
        }

        if (runner_result == PACKAGE_RUNNER_ERROR) {
            result = PACKAGE_OPEN_ERROR;
            break;
        }
    }

    if (directory_changed && previous_directory_saved) {
        if (shell_set_current_directory(previous_directory) != 0) {
            return PACKAGE_OPEN_ERROR;
        }
    }

    return result;
}
