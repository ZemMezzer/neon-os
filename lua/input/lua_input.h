#pragma once

#include "lua.h"

int luaopen_input(lua_State* state);

void lua_input_frame_presented(void);
