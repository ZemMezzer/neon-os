#include "mkdir.h"

#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "neon_fs.h"

int bin_mkdir_main(int argc, char** argv) {
    char path[SHELL_PATH_MAX];

    if (argc < 2) {
        shell_print_error("usage: mkdir <path>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    if (create_directory(path) != FR_OK) {
        shell_print_error("mkdir failed");
        return -1;
    }

    return 0;
}
