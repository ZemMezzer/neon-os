#include "cat.h"

#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "console.h"
#include "neon_fs.h"

int bin_cat_main(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    NeonFsFile file;
    char buffer[129];
    FRESULT result;

    if (argc < 2) {
        shell_print_error("usage: cat <file>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    result = neon_fs_file_open(
        &file,
        path,
        NEON_FS_FILE_OPEN_READ
    );

    if (result != FR_OK) {
        shell_print_error("cannot open file");
        return -1;
    }

    for (;;) {
        UINT read_count = 0;

        result = neon_fs_file_read(
            &file,
            buffer,
            (UINT)(sizeof(buffer) - 1U),
            &read_count
        );

        if (result != FR_OK) {
            (void)neon_fs_file_close(&file);
            shell_print_error("cannot read file");
            return -1;
        }

        if (read_count == 0) {
            break;
        }

        buffer[read_count] = '\0';
        console_write(buffer);
    }

    if (neon_fs_file_close(&file) != FR_OK) {
        shell_print_error("cannot close file");
        return -1;
    }

    console_write("\n");
    return 0;
}
