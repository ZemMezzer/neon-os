#include "shell_script.h"

#include "shell_commands.h"
#include "shell_io.h"
#include "shell_limits.h"
#include "shell_path.h"
#include "shell_util.h"

#include "console.h"
#include "neon_fs.h"

static int shell_script_depth = 0;

int shell_script_is_active(void) {
    return shell_script_depth > 0;
}

int shell_run_script(const char* input_path) {
    char path[SHELL_PATH_MAX];
    char previous_directory[SHELL_PATH_MAX];
    char script_directory[SHELL_PATH_MAX];
    char line[SHELL_LINE_MAX];
    NeonFsFile file;
    FRESULT result;
    int script_status = 0;
    int directory_changed = 0;

    if (shell_script_depth >= SHELL_SCRIPT_MAX_DEPTH) {
        shell_print_error("script nesting limit reached");
        return -1;
    }

    if (shell_resolve_path(input_path, path, sizeof(path)) != 0) {
        shell_print_error("script path too long");
        return -1;
    }

    result = neon_fs_file_open(
        &file,
        path,
        NEON_FS_FILE_OPEN_READ
    );

    if (result != FR_OK) {
        shell_print_error("cannot open script");
        return -1;
    }

    if (
        shell_get_current_directory(
            previous_directory,
            sizeof(previous_directory)
        ) != 0
    ) {
        (void)neon_fs_file_close(&file);
        shell_print_error("cannot read current directory");
        return -1;
    }

    shell_text_copy(script_directory, sizeof(script_directory), path);
    shell_path_parent(script_directory);

    if (shell_set_current_directory(script_directory) != 0) {
        (void)neon_fs_file_close(&file);
        shell_print_error("cannot enter script directory");
        return -1;
    }

    directory_changed = 1;
    shell_script_depth++;

    for (;;) {
        int line_result = shell_file_read_line(&file, line, sizeof(line));

        if (line_result == 0) {
            break;
        }

        if (line_result < 0) {
            (void)neon_fs_file_close(&file);
            shell_script_depth--;

            if (directory_changed) {
                (void)shell_set_current_directory(previous_directory);
            }

            shell_print_error("cannot read script");
            return -1;
        }

        {
            int status;

            shell_text_trim_script_line(line);

            if (line[0] == '\0' || line[0] == '#') {
                continue;
            }

            console_write("> ");
            console_write(line);
            console_write("\n");

            status = shell_commands_execute(line);

            if (status != 0) {
                script_status = status;
            }
        }
    }

    if (neon_fs_file_close(&file) != FR_OK) {
        shell_script_depth--;

        if (directory_changed) {
            (void)shell_set_current_directory(previous_directory);
        }

        shell_print_error("cannot close script");
        return -1;
    }

    shell_script_depth--;

    if (directory_changed) {
        (void)shell_set_current_directory(previous_directory);
    }

    return script_status;
}
