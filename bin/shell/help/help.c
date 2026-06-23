#include "help.h"

#include "shell_commands.h"

#include "console.h"

static int bin_help_print_one(
    const char* name,
    const char* help,
    void* user_data
) {
    (void)user_data;

    console_write("  ");
    console_write(name);
    console_write(" - ");
    console_write(help);
    console_write("\n");

    return 0;
}

int bin_help_main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    console_write("Commands:\n");

    return shell_visit_commands(bin_help_print_one, 0);
}
