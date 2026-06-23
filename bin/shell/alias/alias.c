#include "alias.h"

#include "shell_alias.h"
#include "shell_util.h"

#include "console.h"

int bin_alias_main(int argc, char** argv) {
    char extension[SHELL_ALIAS_EXTENSION_MAX];
    ShellAliasStatus status;

    if (argc < 2) {
        shell_print_error(
            "usage: alias add <extension> <application> | alias remove <extension>"
        );
        return -1;
    }

    if (shell_text_equal(argv[1], "add")) {
        if (argc != 4) {
            shell_print_error("usage: alias add <extension> <application>");
            return -1;
        }

        status = shell_alias_add(
            argv[2],
            argv[3],
            extension,
            sizeof(extension)
        );

        if (status == SHELL_ALIAS_STATUS_TOO_LONG) {
            shell_print_error("extension is too long");
            return -1;
        }

        if (status == SHELL_ALIAS_STATUS_INVALID_EXTENSION) {
            shell_print_error("invalid extension");
            return -1;
        }

        if (status == SHELL_ALIAS_STATUS_INVALID_APPLICATION) {
            shell_print_error("application must be one command name");
            return -1;
        }

        if (
            status != SHELL_ALIAS_STATUS_OK &&
            status != SHELL_ALIAS_STATUS_UPDATED
        ) {
            shell_print_error("cannot write ALIAS.txt");
            return -1;
        }

        console_write("alias: ");
        console_write(
            status == SHELL_ALIAS_STATUS_UPDATED
                ? "updated ."
                : "added ."
        );
        console_write(extension);
        console_write(" -> ");
        console_write(argv[3]);
        console_write("\n");
        return 0;
    }

    if (shell_text_equal(argv[1], "remove")) {
        if (argc != 3) {
            shell_print_error("usage: alias remove <extension>");
            return -1;
        }

        status = shell_alias_remove(
            argv[2],
            extension,
            sizeof(extension)
        );

        if (status == SHELL_ALIAS_STATUS_TOO_LONG) {
            shell_print_error("extension is too long");
            return -1;
        }

        if (status == SHELL_ALIAS_STATUS_INVALID_EXTENSION) {
            shell_print_error("invalid extension");
            return -1;
        }

        if (status == SHELL_ALIAS_STATUS_FILE_NOT_FOUND) {
            shell_print_error("ALIAS.txt not found");
            return -1;
        }

        if (status == SHELL_ALIAS_STATUS_MAPPING_NOT_FOUND) {
            console_write("alias: no association for .");
            console_write(extension);
            console_write("\n");
            return -1;
        }

        if (status != SHELL_ALIAS_STATUS_OK) {
            shell_print_error("cannot write ALIAS.txt");
            return -1;
        }

        console_write("alias: removed .");
        console_write(extension);
        console_write("\n");
        return 0;
    }

    shell_print_error(
        "usage: alias add <extension> <application> | alias remove <extension>"
    );
    return -1;
}
