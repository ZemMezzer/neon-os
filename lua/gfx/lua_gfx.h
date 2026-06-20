#pragma once

#include "lua.h"

/*
    Built-in NeonOS framebuffer module.

    Registered by lua_runner.c as:
        luaL_requiref(L, "gfx", luaopen_gfx, 1);

    Lua side:
        local gfx = require("gfx")
*/
int luaopen_gfx(lua_State* state);
