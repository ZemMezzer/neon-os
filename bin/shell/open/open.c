#include "open.h"

#include "shell_alias.h"
#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_path.h"
#include "shell_script.h"
#include "shell_util.h"

#if PACKAGES_ENABLED
#include "package_runner.h"
#endif

#include "console.h"

int bin_open_main(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    char extension[SHELL_ALIAS_EXTENSION_MAX];
    char application[SHELL_PATH_MAX];
    char command[SHELL_LINE_MAX];
    int found;
    int extension_result;
    int alias_result;

    if (argc < 2) {
        shell_print_error("usage: open <file-or-package> [arguments...]");
        return -1;
    }

    found = shell_find_path(argv[1], path, sizeof(path));

    if (found < 0) {
        shell_print_error("cannot search PATH");
        return -1;
    }

    if (found == 0) {
        shell_print_error("file or package not found");
        return -1;
    }

    if (shell_path_is_directory(path)) {
#if PACKAGES_ENABLED
        int program_status = 1;
        PackageOpenResult package_result;
        char launch_directory[SHELL_PATH_MAX];
        char* package_argv[SHELL_MAX_ARGS];
        int package_argc = argc - 2;
        int index;

        if (
            shell_script_is_active() &&
            shell_text_equal(argv[1], "terminal")
        ) {
            if (
                shell_get_current_directory(
                    launch_directory,
                    sizeof(launch_directory)
                ) != 0 ||
                package_argc + 2 > SHELL_MAX_ARGS
            ) {
                shell_print_error("cannot prepare terminal directory");
                return -1;
            }

            package_argv[0] = "--cwd";
            package_argv[1] = launch_directory;

            for (index = 0; index < package_argc; index++) {
                package_argv[index + 2] = argv[index + 2];
            }

            package_argc += 2;
        } else {
            for (index = 0; index < package_argc; index++) {
                package_argv[index] = argv[index + 2];
            }
        }

        package_result = package_runner_open(
            path,
            package_argc,
            package_argv,
            &program_status
        );

        if (package_result == PACKAGE_OPEN_STARTED) {
            return program_status;
        }

        if (package_result == PACKAGE_OPEN_NOT_PACKAGE) {
            shell_print_error("directory is not an installed package");
            return -1;
        }

        if (package_result == PACKAGE_OPEN_NO_RUNNER) {
            shell_print_error("package has no supported runner");
            return -1;
        }

        shell_print_error("package runner failed");
        return -1;
#else
        shell_print_error("package support is disabled");
        return -1;
#endif
    }

    if (!shell_path_is_regular_file(path)) {
        shell_print_error("path is neither a file nor a package");
        return -1;
    }

    if (argc != 2) {
        shell_print_error("file associations do not accept arguments");
        return -1;
    }

    extension_result = shell_alias_get_path_extension(
        path,
        extension,
        sizeof(extension)
    );

    if (extension_result < 0) {
        shell_print_error("file extension is too long");
        return -1;
    }

    if (extension_result == 0) {
        shell_print_error("file has no extension");
        return -1;
    }

    alias_result = shell_alias_lookup_application(
        extension,
        application,
        sizeof(application)
    );

    if (alias_result == -2) {
        shell_print_error(
            "no file associations configured (ALIAS.txt not found)"
        );
        return -1;
    }

    if (alias_result < 0) {
        shell_print_error("cannot read ALIAS.txt");
        return -1;
    }

    if (alias_result == 0) {
        console_write("Error: no application associated with .");
        console_write(extension);
        console_write("\n");
        return -1;
    }

    if (
        shell_alias_build_open_command(
            application,
            path,
            command,
            sizeof(command)
        ) != 0
    ) {
        shell_print_error("cannot build application command");
        return -1;
    }

    return shell_commands_execute(command);
}
