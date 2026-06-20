#include "gfx.h"
#include "console.h"
#include "input.h"
#include "stdio.h"
#include "shell.h"

#if LUA_ENABLED
#include "lua_shell.h"
#endif

static void halt(void) {
    while (1) {
        asm volatile("wfe");
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

#ifdef LUA_ENABLED
    lua_shell_register_commands();
#endif

    while (1) {
        shell_update();
    }
}