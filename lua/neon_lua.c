#include "neon_lua.h"
#include "lua_shell.h"

void lua_init(void) {
    lua_shell_register_commands();
}