#include "append.h"

#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "neon_fs.h"

int bin_append_main(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    FRESULT result;

    if (argc < 3) {
        shell_print_error("usage: append <file> <text>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    result = append_file(
        path,
        argv[2],
        (UINT)shell_text_length(argv[2])
    );

    if (result == FR_OK) {
        result = append_file(path, "\n", 1);
    }

    if (result != FR_OK) {
        shell_print_error("cannot write file");
        return -1;
    }

    return 0;
}
