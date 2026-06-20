#include "program_runtime.h"

#include "console.h"
#include "stdlib.h"

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

    /*
        Contexts are strictly nested. Do not accidentally detach a parent
        if a program adapter leaves in the wrong order.
    */
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
        /*
            Standard C uses system(NULL) as a "does a command processor
            exist?" query. A registered shell is sufficient.
        */
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
            asm volatile("wfe");
        }
    }

    context->exit_requested = 1;
    context->exit_status = status & 0xFF;

    /*
        The handler belongs to the active adapter. The Lua handler invokes
        lua_error(); another program type can use a different unwinder.
    */
    context->exit_handler(context, context->exit_status);

    /*
        An exit handler must not return.
    */
    for (;;) {
        asm volatile("wfe");
    }
}


/*
    C library compatibility surface used by loslib.c. These symbols belong
    to the generic program runtime, not to Lua.
*/
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
    /*
        A generic environment store can be added later. Returning NULL is
        the normal libc answer for an undefined variable.
    */
    (void)name;
    return NULL;
}
