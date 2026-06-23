#include "echo.h"

#include "console.h"

int bin_echo_main(int argc, char** argv) {
    int index;

    for (index = 1; index < argc; index++) {
        if (index > 1) {
            console_write(" ");
        }

        console_write(argv[index]);
    }

    console_write("\n");
    return 0;
}
