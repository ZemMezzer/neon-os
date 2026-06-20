#pragma once

#include "lua.h"

/*
    Frame-safe Lua input API:

        local key, mods = input.poll()

        if key and mods.ctrl and key == input.S then
            -- Ctrl+S
        end

    `key` preserves text case. For example Shift+A returns byte 'A'.
    Existing games can keep using:

        local key = input.poll()
        if key == input.Q then ... end

    The keyboard driver is touched at most once between gfx.present() calls.
    Only one pending event is cached for the current visual frame.
*/
int luaopen_input(lua_State* state);

/* Called by lua_gfx.c immediately after gfx.present(). */
void lua_input_frame_presented(void);
