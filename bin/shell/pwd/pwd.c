#include "pwd.h"

#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "console.h"

int bin_pwd_main(int argc, char** argv) {
    char cwd[SHELL_PATH_MAX];

    (void)argc;
    (void)argv;

    if (shell_get_current_directory(cwd, sizeof(cwd)) != 0) {
        shell_print_error("cannot read current directory");
        return -1;
    }

    if (shell_text_equal(cwd, "0:/")) {
        console_write("/\n");
    } else {
        console_write(cwd + 2);
        console_write("\n");
    }

    return 0;
}
