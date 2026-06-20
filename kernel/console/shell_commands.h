#pragma once

typedef int (*ShellCommandFn)(int argc, char** argv);

int shell_register_command(
    const char* name,
    const char* help,
    ShellCommandFn fn
);

int shell_resolve_path(
    const char* input,
    char* output,
    int output_size
);

void shell_commands_init(void);
void shell_commands_execute(const char* line);