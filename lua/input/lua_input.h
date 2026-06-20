#pragma once

#include "lua.h"

/*
    Lua module name: input

    Minimal, frame-safe API:

        local key = input.poll()  -- integer key code or nil
        if key == input.Q then ... end
        if input.pressed(input.LEFT) then ... end

    input.poll()/pressed() touches the keyboard driver at most once between
    gfx.present() calls.  The module keeps only one event for the current
    frame; it intentionally does not maintain a second FIFO.
*/
int luaopen_input(lua_State* state);

/* Called by lua_gfx.c immediately after gfx.present(). */
void lua_input_frame_presented(void);
