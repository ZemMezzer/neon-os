#pragma once

typedef int (*ShellCommandFn)(int argc, char** argv);

typedef int (*ShellCommandFallbackFn)(
    int argc,
    char** argv,
    int* out_status
);

int shell_register_command(
    const char* name,
    const char* help,
    ShellCommandFn fn
);

int shell_set_command_fallback(ShellCommandFallbackFn fn);

int shell_resolve_path(
    const char* input,
    char* out,
    int out_size
);

int shell_commands_execute(const char* line);
void shell_commands_init(void);
