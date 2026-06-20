#include <stddef.h>

#include "input.h"
#include "lua_input.h"

#include "lauxlib.h"

/*
    This module intentionally keeps only one event for a visual frame.
    The kernel input queue remains the only FIFO, so Lua cannot accidentally
    create a second event queue or destabilize keyboard polling.
*/
#define LUA_INPUT_CODE_LEFT       0x100
#define LUA_INPUT_CODE_RIGHT      0x101
#define LUA_INPUT_CODE_UP         0x102
#define LUA_INPUT_CODE_DOWN       0x103
#define LUA_INPUT_CODE_HOME       0x104
#define LUA_INPUT_CODE_END        0x105
#define LUA_INPUT_CODE_DELETE     0x106
#define LUA_INPUT_CODE_PAGE_UP    0x107
#define LUA_INPUT_CODE_PAGE_DOWN  0x108
#define LUA_INPUT_CODE_INSERT     0x109
#define LUA_INPUT_CODE_F2         0x10A
#define LUA_INPUT_CODE_ESCAPE     0x10B

static InputEvent lua_input_frame_event;
static int lua_input_frame_checked = 0;
static int lua_input_frame_has_event = 0;

static int lua_input_event_code(const InputEvent* event) {
    if (event == NULL) {
        return 0;
    }

    if (event->type == INPUT_EVENT_CHAR) {
        /*
            Preserve actual character case and punctuation. This is essential
            for text editors. Games that ignore Shift still receive their
            familiar lower-case letters in ordinary use.
        */
        return (int)(unsigned char)event->ch;
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

    if (event->key == INPUT_KEY_PAGE_UP) {
        return LUA_INPUT_CODE_PAGE_UP;
    }

    if (event->key == INPUT_KEY_PAGE_DOWN) {
        return LUA_INPUT_CODE_PAGE_DOWN;
    }

    if (event->key == INPUT_KEY_INSERT) {
        return LUA_INPUT_CODE_INSERT;
    }

    if (event->key == INPUT_KEY_F2) {
        return LUA_INPUT_CODE_F2;
    }

    if (event->key == INPUT_KEY_ESCAPE) {
        return LUA_INPUT_CODE_ESCAPE;
    }

    return 0;
}

static int lua_input_normalize_letter(int value) {
    if (value >= 'A' && value <= 'Z') {
        return value - 'A' + 'a';
    }

    return value;
}

static void lua_input_push_modifiers(lua_State* state, uint8_t modifiers) {
    lua_createtable(state, 0, 4);

    lua_pushboolean(state, (modifiers & INPUT_MOD_SHIFT) != 0);
    lua_setfield(state, -2, "shift");

    lua_pushboolean(state, (modifiers & INPUT_MOD_CTRL) != 0);
    lua_setfield(state, -2, "ctrl");

    lua_pushboolean(state, (modifiers & INPUT_MOD_ALT) != 0);
    lua_setfield(state, -2, "alt");

    lua_pushinteger(state, (lua_Integer)modifiers);
    lua_setfield(state, -2, "mask");
}

/*
    At most one driver update happens per visual frame. The first Lua query
    after gfx.present() fetches one item from the normal kernel queue; every
    later Lua query inspects only this cached event.
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

/*
    input.poll() -> integer, modifiers_table | nil

    Example:
        local key, mods = input.poll()
        if key and mods.ctrl and key == input.S then ...
*/
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
    lua_input_push_modifiers(state, lua_input_frame_event.modifiers);
    return 2;
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

    For control bindings, A and Shift+A are treated as the same letter.
    The actual upper/lower case remains available from input.poll().
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

    if (key_code != 0 &&
        lua_input_normalize_letter(key_code) ==
            lua_input_normalize_letter((int)wanted)) {
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
    { NULL,          NULL }
};

void lua_input_frame_presented(void) {
    /*
        Start a new input frame and drop the one event the Lua program chose
        not to read. The next poll() performs the next driver update.
    */
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
    lua_input_set_constant(state, "PAGE_UP", LUA_INPUT_CODE_PAGE_UP);
    lua_input_set_constant(state, "PAGE_DOWN", LUA_INPUT_CODE_PAGE_DOWN);
    lua_input_set_constant(state, "INSERT", LUA_INPUT_CODE_INSERT);
    lua_input_set_constant(state, "F2", LUA_INPUT_CODE_F2);
    lua_input_set_constant(state, "ESCAPE", LUA_INPUT_CODE_ESCAPE);

    lua_input_set_constant(state, "ENTER", '\n');
    lua_input_set_constant(state, "BACKSPACE", '\b');
    lua_input_set_constant(state, "TAB", '\t');
    lua_input_set_constant(state, "SPACE", ' ');

    lua_input_set_constant(state, "MOD_SHIFT", INPUT_MOD_SHIFT);
    lua_input_set_constant(state, "MOD_CTRL", INPUT_MOD_CTRL);
    lua_input_set_constant(state, "MOD_ALT", INPUT_MOD_ALT);

    return 1;
}
