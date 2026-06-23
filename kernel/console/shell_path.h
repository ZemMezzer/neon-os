#pragma once

#include "shell_commands.h"

typedef enum ShellPathStatus {
    SHELL_PATH_STATUS_OK = 0,
    SHELL_PATH_STATUS_ALREADY_PRESENT = 1,
    SHELL_PATH_STATUS_NOT_FOUND = 2,
    SHELL_PATH_STATUS_NOT_DIRECTORY = 3,
    SHELL_PATH_STATUS_TOO_MANY_ENTRIES = 4,
    SHELL_PATH_STATUS_INVALID = 5,
    SHELL_PATH_STATUS_IO_ERROR = -1
} ShellPathStatus;

typedef int (*ShellPathEntryVisitor)(
    const char* stored_entry,
    const char* absolute_path,
    int path_index,
    void* user_data
);

int shell_path_is_directory(const char* path);
int shell_path_is_regular_file(const char* path);

void shell_path_parent(char* absolute_path);

int shell_path_visit_entries(
    ShellPathEntryVisitor visitor,
    void* user_data
);

ShellPathStatus shell_path_add_directory(
    const char* input,
    char* stored_entry,
    int stored_entry_size
);

ShellPathStatus shell_path_remove_directory(const char* input);
