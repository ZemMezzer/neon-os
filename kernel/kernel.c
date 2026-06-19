#include "gfx.h"
#include "console.h"
#include "input.h"
#include "stdio.h"
#include "shell.h"

static void halt(void) {
    while (1) {
        asm volatile("nop");
    }
}

void kernel_main(void) {
    gfx_init();
    console_init();

    if (stdio_init() != 0) {
        console_write("stdio_init failed\n");
        halt();
    }

    shell_init();

    while (1) {
        shell_update();
    }
}