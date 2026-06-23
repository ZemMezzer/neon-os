#ifndef SHELL_COMMANDS_H
#define SHELL_COMMANDS_H

#include "shell_limits.h"

/*
 * Commands are linked into the kernel at the moment.  The name "command"
 * deliberately does not imply that a command is a disk executable yet.
 */
typedef int (*ShellCommandFn)(int argc, char** argv);

typedef int (*ShellCommandFallbackFn)(
    int argc,
    char** argv,
    int* out_status
);

typedef int (*ShellCommandVisitor)(
    const char* name,
    const char* help,
    void* user_data
);

typedef int (*ShellPathDirectoryVisitor)(
    const char* directory,
    int path_index,
    void* user_data
);

int shell_register_command(
    const char* name,
    const char* help,
    ShellCommandFn fn
);

int shell_set_command_fallback(ShellCommandFallbackFn fn);

int shell_visit_commands(
    ShellCommandVisitor visitor,
    void* user_data
);

int shell_resolve_path(
    const char* input,
    char* output,
    int output_size
);

int shell_get_current_directory(
    char* output,
    int output_size
);

int shell_set_current_directory(const char* input);

void shell_picker_clear_result(void);

int shell_picker_set_result(const char* path);

int shell_picker_take_result(
    char* output,
    int output_size
);

int shell_find_command_file(
    const char* filename,
    char* output,
    int output_size
);

int shell_find_path(
    const char* name,
    char* output,
    int output_size
);

int shell_visit_path_directories(
    ShellPathDirectoryVisitor visitor,
    void* user_data
);

int shell_is_direct_child_of_path(const char* absolute_path);

int shell_run_script(const char* input_path);

int shell_commands_execute(const char* line);

void shell_commands_init(void);

#endif /* SHELL_COMMANDS_H */
