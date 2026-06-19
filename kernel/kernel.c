#include "gfx.h"
#include "console.h"
#include "shell.h"

void kernel_main(void) {
    gfx_init();

    console_init();
    shell_init();

    while (1) {
        shell_update();

        asm volatile("nop");
    }
}