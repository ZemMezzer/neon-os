#include "cd.h"

#include "shell_commands.h"
#include "shell_util.h"

int bin_cd_main(int argc, char** argv) {
    const char* target;

    if (argc < 2) {
        target = "0:/";
    } else {
        target = argv[1];
    }

    if (shell_set_current_directory(target) != 0) {
        shell_print_error("directory not found");
        return -1;
    }

    return 0;
}
