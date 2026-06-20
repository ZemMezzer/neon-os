#include <stddef.h>

#include "input.h"
#include "lua_input.h"

#include "lauxlib.h"

/*
    Keep this module deliberately small while the input path is being
    stabilized.  The kernel input queue remains the only FIFO.  Lua caches at
    most one event for a visual frame, avoiding an additional queue, event
    shifting, and dynamic Lua string creation for every pressed key.
*/
#define LUA_INPUT_CODE_LEFT    0x100
#define LUA_INPUT_CODE_RIGHT   0x101
#define LUA_INPUT_CODE_UP      0x102
#define LUA_INPUT_CODE_DOWN    0x103
#define LUA_INPUT_CODE_HOME    0x104
#define LUA_INPUT_CODE_END     0x105
#define LUA_INPUT_CODE_DELETE  0x106

static InputEvent lua_input_frame_event;
static int lua_input_frame_checked = 0;
static int lua_input_frame_has_event = 0;


static int lua_input_event_code(const InputEvent* event) {
    unsigned char character;

    if (event == NULL) {
        return 0;
    }

    if (event->type == INPUT_EVENT_CHAR) {
        character = (unsigned char)event->ch;

        /* Game/control bindings should not distinguish Q from Shift+Q. */
        if (character >= (unsigned char)'A' && character <= (unsigned char)'Z') {
            character = (unsigned char)(character - (unsigned char)'A' + (unsigned char)'a');
        }

        return (int)character;
    }

    if (event->type != INPUT_EVENT_KEY) {
        return 0;
    }

    if (event->key == INPUT_KEY_LEFT) {
        return LUA_INPUT_CODE_LEFT;
    }

    if (event->key == INPUT_KEY_RIGHT) {
        return LUA_INPUT_CODE_RIGHT;
    }

    if (event->key == INPUT_KEY_UP) {
        return LUA_INPUT_CODE_UP;
    }

    if (event->key == INPUT_KEY_DOWN) {
        return LUA_INPUT_CODE_DOWN;
    }

    if (event->key == INPUT_KEY_HOME) {
        return LUA_INPUT_CODE_HOME;
    }

    if (event->key == INPUT_KEY_END) {
        return LUA_INPUT_CODE_END;
    }

    if (event->key == INPUT_KEY_DELETE) {
        return LUA_INPUT_CODE_DELETE;
    }

    return 0;
}


/*
    At most one driver update happens in a frame.  The first Lua input query
    after gfx.present() fetches one item from the normal kernel queue; every
    other Lua input query only looks at this cached item.
*/
static void lua_input_begin_frame_if_needed(void) {
    if (lua_input_frame_checked) {
        return;
    }

    lua_input_frame_checked = 1;
    lua_input_frame_has_event = 0;

    input_update();

    if (input_poll(&lua_input_frame_event)) {
        lua_input_frame_has_event = 1;
    }
}


/* input.poll() -> integer | nil */
static int lua_input_poll(lua_State* state) {
    int key_code;

    lua_input_begin_frame_if_needed();

    if (!lua_input_frame_has_event) {
        lua_pushnil(state);
        return 1;
    }

    key_code = lua_input_event_code(&lua_input_frame_event);
    lua_input_frame_has_event = 0;

    if (key_code == 0) {
        lua_pushnil(state);
        return 1;
    }

    lua_pushinteger(state, (lua_Integer)key_code);
    return 1;
}


/* input.any_pressed() -> boolean */
static int lua_input_any_pressed(lua_State* state) {
    lua_input_begin_frame_if_needed();

    if (!lua_input_frame_has_event) {
        lua_pushboolean(state, 0);
        return 1;
    }

    lua_input_frame_has_event = 0;
    lua_pushboolean(state, 1);
    return 1;
}


/*
    input.pressed(key_code) -> boolean

    It does not discard an unrelated event.  Thus a program may safely write:

        if input.pressed(input.Q) then ... end
        if input.pressed(input.LEFT) then ... end

    Any non-matching event is discarded only at the next gfx.present().
*/
static int lua_input_pressed(lua_State* state) {
    lua_Integer wanted;
    int key_code;

    wanted = luaL_checkinteger(state, 1);

    lua_input_begin_frame_if_needed();

    if (!lua_input_frame_has_event) {
        lua_pushboolean(state, 0);
        return 1;
    }

    key_code = lua_input_event_code(&lua_input_frame_event);

    if (key_code != 0 && (lua_Integer)key_code == wanted) {
        lua_input_frame_has_event = 0;
        lua_pushboolean(state, 1);
        return 1;
    }

    lua_pushboolean(state, 0);
    return 1;
}


static void lua_input_set_constant(
    lua_State* state,
    const char* name,
    int value
) {
    lua_pushinteger(state, (lua_Integer)value);
    lua_setfield(state, -2, name);
}


static void lua_input_set_letter_constants(lua_State* state) {
    char name[2];
    char letter;

    name[1] = '\0';

    for (letter = 'A'; letter <= 'Z'; letter++) {
        name[0] = letter;
        lua_input_set_constant(
            state,
            name,
            (int)(letter - 'A' + 'a')
        );
    }
}


static const luaL_Reg lua_input_functions[] = {
    { "poll",        lua_input_poll },
    { "any_pressed", lua_input_any_pressed },
    { "pressed",     lua_input_pressed },
    { "key_pressed", lua_input_pressed },
    { NULL,            NULL }
};


void lua_input_frame_presented(void) {
    /* Start a new input frame and discard an event the script ignored. */
    lua_input_frame_checked = 0;
    lua_input_frame_has_event = 0;
}


int luaopen_input(lua_State* state) {
    lua_input_frame_checked = 0;
    lua_input_frame_has_event = 0;

    luaL_newlib(state, lua_input_functions);

    lua_input_set_letter_constants(state);

    lua_input_set_constant(state, "LEFT", LUA_INPUT_CODE_LEFT);
    lua_input_set_constant(state, "RIGHT", LUA_INPUT_CODE_RIGHT);
    lua_input_set_constant(state, "UP", LUA_INPUT_CODE_UP);
    lua_input_set_constant(state, "DOWN", LUA_INPUT_CODE_DOWN);
    lua_input_set_constant(state, "HOME", LUA_INPUT_CODE_HOME);
    lua_input_set_constant(state, "END", LUA_INPUT_CODE_END);
    lua_input_set_constant(state, "DELETE", LUA_INPUT_CODE_DELETE);

    lua_input_set_constant(state, "ENTER", '\n');
    lua_input_set_constant(state, "BACKSPACE", '\b');
    lua_input_set_constant(state, "TAB", '\t');
    lua_input_set_constant(state, "SPACE", ' ');

    return 1;
}
