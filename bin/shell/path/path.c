#include "path.h"

#include "shell_path.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "console.h"

static int bin_path_print_entry(
    const char* stored_entry,
    const char* absolute_path,
    int path_index,
    void* user_data
) {
    (void)absolute_path;
    (void)path_index;
    (void)user_data;

    console_write("  ");
    console_write(stored_entry);
    console_write("\n");
    return 0;
}

int bin_path_main(int argc, char** argv) {
    if (argc == 1) {
        console_write("PATH:\n");

        if (shell_path_visit_entries(bin_path_print_entry, 0) != 0) {
            shell_print_error("cannot read PATH.txt");
            return -1;
        }

        return 0;
    }

    if (shell_text_equal(argv[1], "add")) {
        char stored[SHELL_PATH_MAX];
        ShellPathStatus status;

        if (argc != 3) {
            shell_print_error("usage: path add <directory>");
            return -1;
        }

        status = shell_path_add_directory(
            argv[2],
            stored,
            sizeof(stored)
        );

        if (status == SHELL_PATH_STATUS_ALREADY_PRESENT) {
            console_write("path: already present\n");
            return 0;
        }

        if (status == SHELL_PATH_STATUS_NOT_DIRECTORY) {
            shell_print_error("directory not found");
            return -1;
        }

        if (status == SHELL_PATH_STATUS_TOO_MANY_ENTRIES) {
            shell_print_error("too many PATH entries");
            return -1;
        }

        if (status == SHELL_PATH_STATUS_INVALID) {
            shell_print_error("path too long");
            return -1;
        }

        if (status != SHELL_PATH_STATUS_OK) {
            shell_print_error("cannot write PATH.txt");
            return -1;
        }

        console_write("path: added ");
        console_write(stored);
        console_write("\n");
        return 0;
    }

    if (shell_text_equal(argv[1], "remove")) {
        ShellPathStatus status;

        if (argc != 3) {
            shell_print_error("usage: path remove <directory>");
            return -1;
        }

        status = shell_path_remove_directory(argv[2]);

        if (status == SHELL_PATH_STATUS_NOT_FOUND) {
            shell_print_error("PATH entry not found");
            return -1;
        }

        if (status == SHELL_PATH_STATUS_INVALID) {
            shell_print_error("path too long");
            return -1;
        }

        if (status != SHELL_PATH_STATUS_OK) {
            shell_print_error("cannot write PATH.txt");
            return -1;
        }

        console_write("path: removed\n");
        return 0;
    }

    shell_print_error("usage: path [add <directory> | remove <directory>]");
    return -1;
}
