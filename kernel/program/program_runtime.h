#pragma once


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

void program_exit(int status) __attribute__((noreturn));

void program_set_command_executor(ProgramCommandExecutor executor);
int program_execute_command(const char* command);
