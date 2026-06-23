#pragma once

#include "shell_limits.h"

typedef enum ShellAliasStatus {
    SHELL_ALIAS_STATUS_OK = 0,
    SHELL_ALIAS_STATUS_UPDATED = 1,
    SHELL_ALIAS_STATUS_MAPPING_NOT_FOUND = 2,
    SHELL_ALIAS_STATUS_FILE_NOT_FOUND = 3,
    SHELL_ALIAS_STATUS_INVALID_EXTENSION = 4,
    SHELL_ALIAS_STATUS_INVALID_APPLICATION = 5,
    SHELL_ALIAS_STATUS_TOO_LONG = 6,
    SHELL_ALIAS_STATUS_IO_ERROR = -1
} ShellAliasStatus;

ShellAliasStatus shell_alias_add(
    const char* extension,
    const char* application,
    char* normalized_extension,
    int normalized_extension_size
);

ShellAliasStatus shell_alias_remove(
    const char* extension,
    char* normalized_extension,
    int normalized_extension_size
);

/*
 * Returns 1 when the path has a valid extension, 0 when it has none,
 * and -1 if the extension does not fit in output.
 */
int shell_alias_get_path_extension(
    const char* path,
    char* output,
    int output_size
);

/*
 * Returns 1 for a mapping, 0 for no mapping, -2 when ALIAS.txt does not
 * exist, and -1 for another read/format error.
 */
int shell_alias_lookup_application(
    const char* extension,
    char* output,
    int output_size
);

int shell_alias_build_open_command(
    const char* application,
    const char* path,
    char* output,
    int output_size
);
