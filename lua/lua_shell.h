#pragma once

/*
    Registers the optional `lua <script>` shell command.

    This header is only included by the kernel when LUA_ENABLED is defined.
*/
void lua_shell_register_commands(void);
