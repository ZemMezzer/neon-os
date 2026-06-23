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

static int bin_open_associated_application(
    char* application,
    char* file_path,
    int argc,
    char** argv
) {
    char application_path[SHELL_PATH_MAX];
    char* application_argv[SHELL_MAX_ARGS];
    int application_found;
    int index;

    if (
        application == 0 ||
        file_path == 0 ||
        argc < 2 ||
        argc > SHELL_MAX_ARGS ||
        argv == 0
    ) {
        shell_print_error("cannot prepare associated application");
        return -1;
    }

    application_argv[0] = application;
    application_argv[1] = file_path;

    for (index = 2; index < argc; index++) {
        application_argv[index] = argv[index];
    }

    application_found = shell_find_path(
        application,
        application_path,
        sizeof(application_path)
    );

    if (application_found < 0) {
        shell_print_error("cannot search PATH for associated application");
        return -1;
    }

    if (
        application_found > 0 &&
        shell_path_is_directory(application_path)
    ) {
#if PACKAGES_ENABLED
        int program_status = 1;
        PackageOpenResult package_result;

        package_result = package_runner_open(
            application_path,
            argc - 1,
            &application_argv[1],
            &program_status
        );

        if (package_result == PACKAGE_OPEN_STARTED) {
            return program_status;
        }

        if (package_result == PACKAGE_OPEN_NOT_PACKAGE) {
            shell_print_error(
                "associated application is not an installed package"
            );
            return -1;
        }

        if (package_result == PACKAGE_OPEN_NO_RUNNER) {
            shell_print_error(
                "associated application has no supported runner"
            );
            return -1;
        }

        shell_print_error("associated package runner failed");
        return -1;
#else
        shell_print_error("package support is disabled");
        return -1;
#endif
    }

    /*
     * Regular application files, including lua.lua, are launched by the
     * shell fallback.  This keeps the arguments as an argv array and does
     * not create a limited SHELL_LINE_MAX command string.
     */
    return shell_commands_execute_argv(argc, application_argv);
}

int bin_open_main(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    char extension[SHELL_ALIAS_EXTENSION_MAX];
    char application[SHELL_PATH_MAX];
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

    return bin_open_associated_application(
        application,
        path,
        argc,
        argv
    );
}
