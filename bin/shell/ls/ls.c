#include "ls.h"

#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "console.h"
#include "neon_fs.h"

int bin_ls_main(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    NeonFsDirectory directory;
    FRESULT result;

    if (argc >= 2) {
        if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
            shell_print_error("path too long");
            return -1;
        }
    } else {
        if (shell_get_current_directory(path, sizeof(path)) != 0) {
            shell_print_error("cannot read current directory");
            return -1;
        }
    }

    result = open_directory(&directory, path);

    if (result != FR_OK) {
        shell_print_error("cannot open directory");
        return -1;
    }

    for (;;) {
        NeonFsEntry entry;
        int end = 0;

        result = read_directory(&directory, &entry, &end);

        if (result != FR_OK) {
            (void)close_directory(&directory);
            shell_print_error("cannot read directory");
            return -1;
        }

        if (end) {
            break;
        }

        console_write((entry.attributes & AM_DIR) ? "[D] " : "    ");
        console_write(entry.name);
        console_write("\n");
    }

    if (close_directory(&directory) != FR_OK) {
        shell_print_error("cannot close directory");
        return -1;
    }

    return 0;
}
