#include "program_runtime.h"

#include "console.h"
#include "stdlib.h"
#include "arch.h"

static ProgramContext* active_program = NULL;
static ProgramCommandExecutor command_executor = NULL;


void program_context_enter(
    ProgramContext* context,
    ProgramExitHandler exit_handler,
    void* userdata
) {
    if (context == NULL) {
        return;
    }

    context->parent = active_program;
    context->exit_handler = exit_handler;
    context->userdata = userdata;
    context->exit_requested = 0;
    context->exit_status = 0;

    active_program = context;
}


void program_context_leave(ProgramContext* context) {
    if (context == NULL) {
        return;
    }

    if (active_program == context) {
        active_program = context->parent;
    }

    context->parent = NULL;
    context->exit_handler = NULL;
    context->userdata = NULL;
}


int program_context_exit_requested(const ProgramContext* context) {
    return context != NULL && context->exit_requested;
}


int program_context_exit_status(const ProgramContext* context) {
    return context == NULL ? 1 : context->exit_status;
}


void* program_context_userdata(const ProgramContext* context) {
    return context == NULL ? NULL : context->userdata;
}


void program_set_command_executor(ProgramCommandExecutor executor) {
    command_executor = executor;
}


int program_execute_command(const char* command) {
    if (command == NULL) {
        return command_executor != NULL ? 1 : 0;
    }

    if (command_executor == NULL) {
        return -1;
    }

    return command_executor(command);
}


void program_exit(int status) {
    ProgramContext* context = active_program;

    if (context == NULL || context->exit_handler == NULL) {
        console_write("fatal: exit() outside a program\n");

        for (;;) {
            arch_wait_for_event();
        }
    }

    context->exit_requested = 1;
    context->exit_status = status & 0xFF;

    context->exit_handler(context, context->exit_status);

    for (;;) {
        arch_wait_for_event();
    }
}


int system(const char* command) {
    int status = program_execute_command(command);

    if (status < 0) {
        return -1;
    }

    return status & 0xFF;
}


void exit(int status) {
    program_exit(status);
}


char* getenv(const char* name) {
    (void)name;
    return NULL;
}
