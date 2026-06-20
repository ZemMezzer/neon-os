#include <stddef.h>

#include "input.h"
#include "lua_input.h"

#include "lauxlib.h"

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
#define LUA_INPUT_CODE_ESCAPE     0x10A

#define LUA_INPUT_CODE_F1         0x120
#define LUA_INPUT_CODE_F2         0x121
#define LUA_INPUT_CODE_F3         0x122
#define LUA_INPUT_CODE_F4         0x123
#define LUA_INPUT_CODE_F5         0x124
#define LUA_INPUT_CODE_F6         0x125
#define LUA_INPUT_CODE_F7         0x126
#define LUA_INPUT_CODE_F8         0x127
#define LUA_INPUT_CODE_F9         0x128
#define LUA_INPUT_CODE_F10        0x129
#define LUA_INPUT_CODE_F11        0x12A
#define LUA_INPUT_CODE_F12        0x12B

static InputEvent lua_input_frame_event;
static int lua_input_frame_checked = 0;
static int lua_input_frame_has_event = 0;


static int lua_input_function_key_code(InputKey key) {
    if (key >= INPUT_KEY_F1 && key <= INPUT_KEY_F12) {
        return LUA_INPUT_CODE_F1 + (int)(key - INPUT_KEY_F1);
    }

    return 0;
}


static int lua_input_key_code(InputKey key) {
    int function_key_code;

    switch (key) {
        case INPUT_KEY_LEFT:
            return LUA_INPUT_CODE_LEFT;
        case INPUT_KEY_RIGHT:
            return LUA_INPUT_CODE_RIGHT;
        case INPUT_KEY_UP:
            return LUA_INPUT_CODE_UP;
        case INPUT_KEY_DOWN:
            return LUA_INPUT_CODE_DOWN;
        case INPUT_KEY_HOME:
            return LUA_INPUT_CODE_HOME;
        case INPUT_KEY_END:
            return LUA_INPUT_CODE_END;
        case INPUT_KEY_DELETE:
            return LUA_INPUT_CODE_DELETE;
        case INPUT_KEY_PAGE_UP:
            return LUA_INPUT_CODE_PAGE_UP;
        case INPUT_KEY_PAGE_DOWN:
            return LUA_INPUT_CODE_PAGE_DOWN;
        case INPUT_KEY_INSERT:
            return LUA_INPUT_CODE_INSERT;
        case INPUT_KEY_ESCAPE:
            return LUA_INPUT_CODE_ESCAPE;
        default:
            break;
    }

    function_key_code = lua_input_function_key_code(key);
    return function_key_code;
}


static int lua_input_event_code(
    const InputEvent* event,
    int normalize_letter_case
) {
    unsigned char character;

    if (event == NULL) {
        return 0;
    }

    if (event->type == INPUT_EVENT_CHAR) {
        character = (unsigned char)event->ch;
        if (
            normalize_letter_case &&
            character >= (unsigned char)'A' &&
            character <= (unsigned char)'Z'
        ) {
            character = (unsigned char)(
                character - (unsigned char)'A' + (unsigned char)'a'
            );
        }

        return (int)character;
    }

    if (event->type != INPUT_EVENT_KEY) {
        return 0;
    }

    return lua_input_key_code(event->key);
}


static void lua_input_push_modifiers(
    lua_State* state,
    uint8_t modifiers
) {
    lua_createtable(state, 0, 3);

    lua_pushboolean(state, (modifiers & INPUT_MOD_SHIFT) != 0);
    lua_setfield(state, -2, "shift");

    lua_pushboolean(state, (modifiers & INPUT_MOD_CTRL) != 0);
    lua_setfield(state, -2, "ctrl");

    lua_pushboolean(state, (modifiers & INPUT_MOD_ALT) != 0);
    lua_setfield(state, -2, "alt");
}


static int lua_input_push_poll_result(
    lua_State* state,
    const InputEvent* event,
    int normalize_letter_case
) {
    int key_code;

    key_code = lua_input_event_code(event, normalize_letter_case);

    if (key_code == 0) {
        lua_pushnil(state);
        return 1;
    }

    lua_pushinteger(state, (lua_Integer)key_code);
    lua_input_push_modifiers(state, event->modifiers);
    return 2;
}

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


static int lua_input_poll(lua_State* state) {
    InputEvent event;

    lua_input_begin_frame_if_needed();

    if (!lua_input_frame_has_event) {
        lua_pushnil(state);
        return 1;
    }

    event = lua_input_frame_event;
    lua_input_frame_has_event = 0;
    return lua_input_push_poll_result(state, &event, 0);
}

static int lua_input_poll_latest(lua_State* state) {
    InputEvent event;
    InputEvent latest_event;
    int has_latest_event = 0;

    lua_input_begin_frame_if_needed();

    if (lua_input_frame_has_event) {
        event = lua_input_frame_event;
        lua_input_frame_has_event = 0;

        if (lua_input_event_code(&event, 1) != 0) {
            latest_event = event;
            has_latest_event = 1;
        }
    }

    while (input_poll(&event)) {
        if (lua_input_event_code(&event, 1) != 0) {
            latest_event = event;
            has_latest_event = 1;
        }
    }

    if (!has_latest_event) {
        lua_pushnil(state);
        return 1;
    }

    return lua_input_push_poll_result(state, &latest_event, 1);
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

static int lua_input_pressed(lua_State* state) {
    lua_Integer wanted;
    int key_code;

    wanted = luaL_checkinteger(state, 1);

    lua_input_begin_frame_if_needed();

    if (!lua_input_frame_has_event) {
        lua_pushboolean(state, 0);
        return 1;
    }

    key_code = lua_input_event_code(&lua_input_frame_event, 1);

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


static void lua_input_set_function_key_constants(lua_State* state) {
    static const char* const names[] = {
        "F1",  "F2",  "F3",  "F4",
        "F5",  "F6",  "F7",  "F8",
        "F9",  "F10", "F11", "F12"
    };
    unsigned int index;

    for (index = 0; index < sizeof(names) / sizeof(names[0]); index++) {
        lua_input_set_constant(
            state,
            names[index],
            LUA_INPUT_CODE_F1 + (int)index
        );
    }
}


static const luaL_Reg lua_input_functions[] = {
    { "poll",        lua_input_poll },
    { "poll_latest", lua_input_poll_latest },
    { "any_pressed", lua_input_any_pressed },
    { "pressed",     lua_input_pressed },
    { "key_pressed", lua_input_pressed },
    { NULL,           NULL }
};


void lua_input_frame_presented(void) {
    lua_input_frame_checked = 0;
    lua_input_frame_has_event = 0;
}


int luaopen_input(lua_State* state) {
    lua_input_frame_checked = 0;
    lua_input_frame_has_event = 0;

    luaL_newlib(state, lua_input_functions);

    lua_input_set_letter_constants(state);
    lua_input_set_function_key_constants(state);

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
    lua_input_set_constant(state, "ESCAPE", LUA_INPUT_CODE_ESCAPE);

    lua_input_set_constant(state, "ENTER", '\n');
    lua_input_set_constant(state, "BACKSPACE", '\b');
    lua_input_set_constant(state, "TAB", '\t');
    lua_input_set_constant(state, "SPACE", ' ');

    return 1;
}
