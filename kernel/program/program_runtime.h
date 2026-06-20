#pragma once

/*
    Generic, single-threaded program nesting.

    This layer knows nothing about Lua, the shell parser, or a particular
    executable format. A program adapter installs an exit handler when it
    becomes active. The shell only registers a command executor.
*/

typedef struct ProgramContext ProgramContext;

typedef void (*ProgramExitHandler)(
    ProgramContext* context,
    int status
);

typedef int (*ProgramCommandExecutor)(const char* command);

struct ProgramContext {
    ProgramContext* parent;
    ProgramExitHandler exit_handler;
    void* userdata;

    int exit_requested;
    int exit_status;
};

void program_context_enter(
    ProgramContext* context,
    ProgramExitHandler exit_handler,
    void* userdata
);

void program_context_leave(ProgramContext* context);

int program_context_exit_requested(const ProgramContext* context);
int program_context_exit_status(const ProgramContext* context);
void* program_context_userdata(const ProgramContext* context);

/*
    Terminates the active program. It does not stop the kernel.
    The active program adapter decides how to unwind its own execution.
*/
void program_exit(int status) __attribute__((noreturn));

/*
    The shell registers its synchronous command runner here. libc system()
    calls this generic entry point instead of knowing about the shell.
*/
void program_set_command_executor(ProgramCommandExecutor executor);
int program_execute_command(const char* command);
