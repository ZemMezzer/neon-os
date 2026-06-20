#pragma once

#include "lua.h"

/*
    Built-in Lua module: fs

    This is a small single-threaded filesystem layer over FatFs. It is
    registered by lua_runner.c with luaL_requiref(), so scripts may use either:

        local fs = require("fs")

    or the global table `fs`.
*/
int luaopen_fs(lua_State* state);
