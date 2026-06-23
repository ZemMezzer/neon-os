#include "sh.h"

#include "shell_script.h"
#include "shell_util.h"

int bin_sh_main(int argc, char** argv) {
    if (argc != 2) {
        shell_print_error("usage: sh <script>");
        return -1;
    }

    return shell_run_script(argv[1]);
}
