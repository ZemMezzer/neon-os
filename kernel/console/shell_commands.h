#pragma once

/*
    Shell command callback used for built-in commands such as cd, ls and lua.
*/
typedef int (*ShellCommandFn)(int argc, char** argv);

/*
    Resolver for commands that are not built into the generic shell.
    Return nonzero when the command has been handled and put the process
    exit status in *out_status.
*/
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
