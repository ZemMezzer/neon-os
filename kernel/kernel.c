#if GFX_ENABLED
#include "gfx.h"
#endif

#include "console.h"
#include "input.h"
#include "stdio.h"
#include "shell.h"
#include "arch.h"
#include "neon_fs.h"
#include "shell_commands.h"

#if LUA_ENABLED
#include "neon_lua.h"
#endif

#if BUILTIN_RESOURCES_ENABLED
#include "builtin_apps_install.h"
#endif

static void halt(void) {
    while (1) {
        arch_wait_for_event();
    }
}

void kernel_main(void) {

#if GFX_ENABLED
    gfx_init();
#endif

    console_init();

    if (stdio_init() != 0) {
        console_write("stdio_init failed\n");
        halt();
    }

#if BUILTIN_RESOURCES_ENABLED
    if (builtin_apps_install_if_needed() != 0) {
        console_write("builtin app install failed\n");
    }
#endif

    shell_init();

#ifdef LUA_ENABLED
    lua_init();
#endif

    if(file_exists("0:/.system/scripts/boot.sh")){
        shell_commands_execute("sh 0:/.system/scripts/boot.sh");
    }

    while (1) {
        shell_update();
    }
}