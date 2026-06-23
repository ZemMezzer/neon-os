#include "mv.h"

#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "neon_fs.h"

int bin_mv_main(int argc, char** argv) {
    char old_path[SHELL_PATH_MAX];
    char new_path[SHELL_PATH_MAX];

    if (argc < 3) {
        shell_print_error("usage: mv <old> <new>");
        return -1;
    }

    if (
        shell_resolve_path(argv[1], old_path, sizeof(old_path)) != 0
    ) {
        shell_print_error("old path too long");
        return -1;
    }

    if (
        shell_resolve_path(argv[2], new_path, sizeof(new_path)) != 0
    ) {
        shell_print_error("new path too long");
        return -1;
    }

    if (rename_path(old_path, new_path) != FR_OK) {
        shell_print_error("rename failed");
        return -1;
    }

    return 0;
}
