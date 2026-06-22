#include "package_shell.h"

#include <stddef.h>

#include "console.h"
#include "neon_fs.h"
#include "package_manager.h"
#include "shell_commands.h"

static void package_shell_error(const char* text) {
    console_write("Error: ");
    console_write(text);
    console_write("\n");
}

static int package_shell_string_equal(
    const char* left,
    const char* right
) {
    int index = 0;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }

        index++;
    }

    return left[index] == right[index];
}

static int package_command_install(int argc, char** argv) {
    char archive_path[NEON_FS_PATH_MAX];
    char derived_name[PACKAGE_NAME_MAX];
    const char* package_name;
    PackageStatus status;
    int found;

    if (argc != 2 && argc != 3) {
        package_shell_error(
            "usage: package install <package.npkg> [package-name]"
        );
        return 1;
    }

    found = shell_find_path(
        argv[1],
        archive_path,
        sizeof(archive_path)
    );

    if (found < 0) {
        package_shell_error("cannot search for package archive");
        return 1;
    }

    if (found == 0 || !file_exists(archive_path)) {
        package_shell_error("package archive not found");
        return 1;
    }

    status = package_name_from_archive(
        archive_path,
        derived_name,
        sizeof(derived_name)
    );

    if (status != PACKAGE_OK) {
        package_shell_error("package archive must end in .npkg");
        return 1;
    }

    package_name = argc == 3 ? argv[2] : derived_name;

    if (!package_name_is_valid(package_name)) {
        package_shell_error("invalid package name");
        return 1;
    }

    status = package_install_archive(archive_path, package_name);

    if (
        status != PACKAGE_OK &&
        status != PACKAGE_OK_CLEANUP_PENDING
    ) {
        console_write("package install: ");
        console_write(package_status_string(status));
        console_write("\n");
        return 1;
    }

    console_write("Installed package: ");
    console_write(package_name);

    if (status == PACKAGE_OK_CLEANUP_PENDING) {
        console_write(" (old backup cleanup pending)");
    }

    console_write("\n");
    return 0;
}

static int package_command_uninstall(int argc, char** argv) {
    PackageStatus status;

    if (argc != 2) {
        package_shell_error("usage: package uninstall <package-name>");
        return 1;
    }

    status = package_remove(argv[1]);

    if (status != PACKAGE_OK) {
        console_write("package uninstall: ");
        console_write(package_status_string(status));
        console_write("\n");
        return 1;
    }

    console_write("Uninstalled package: ");
    console_write(argv[1]);
    console_write("\n");
    return 0;
}

typedef struct PackageShellListContext {
    int count;
} PackageShellListContext;

static void package_shell_list_item(
    const PackageInfo* info,
    void* user_data
) {
    PackageShellListContext* context = (PackageShellListContext*)user_data;

    if (info == NULL || context == NULL) {
        return;
    }

    console_write("  ");
    console_write(info->id);
    console_write("\n");
    context->count++;
}

static int package_command_list(int argc, char** argv) {
    PackageShellListContext context;
    PackageStatus status;

    (void)argv;

    if (argc != 1) {
        package_shell_error("usage: package list");
        return 1;
    }

    context.count = 0;

    status = package_list(package_shell_list_item, &context);

    if (status != PACKAGE_OK) {
        console_write("package list: ");
        console_write(package_status_string(status));
        console_write("\n");
        return 1;
    }

    return 0;
}

static int cmd_package(int argc, char** argv) {
    if (argc < 2) {
        package_shell_error(
            "usage: package <install|uninstall|list> ..."
        );
        return 1;
    }

    if (package_shell_string_equal(argv[1], "install")) {
        return package_command_install(argc - 1, argv + 1);
    }

    if (package_shell_string_equal(argv[1], "uninstall")) {
        return package_command_uninstall(argc - 1, argv + 1);
    }

    if (package_shell_string_equal(argv[1], "list")) {
        return package_command_list(argc - 1, argv + 1);
    }

    package_shell_error(
        "usage: package <install|uninstall|list> ..."
    );
    return 1;
}

void package_shell_register_commands(void) {
    (void)package_prepare();

    (void)shell_register_command(
        "package",
        "install, uninstall, or list packages",
        cmd_package
    );
}
