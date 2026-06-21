#ifndef SHELL_COMMANDS_H
#define SHELL_COMMANDS_H

/*
    A built-in shell command. Return 0 on success; non-zero is a command
    status that callers such as shell scripts can inspect.
*/
typedef int (*ShellCommandFn)(int argc, char** argv);

/*
    Called when no built-in command name matched.

    Return non-zero when the fallback handled the command and stores its
    exit status in out_status. Return 0 to let the shell report
    "Unknown command".
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
    char* output,
    int output_size
);

int shell_find_command_file(
    const char* filename,
    char* output,
    int output_size
);

int shell_commands_execute(const char* line);

void shell_commands_init(void);

#endif
