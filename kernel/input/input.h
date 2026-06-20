#pragma once

#include <stdint.h>

typedef enum InputEventType {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_CHAR = 1,
    INPUT_EVENT_KEY  = 2
} InputEventType;

typedef enum InputKey {
    INPUT_KEY_NONE = 0,

    INPUT_KEY_LEFT,
    INPUT_KEY_RIGHT,
    INPUT_KEY_UP,
    INPUT_KEY_DOWN,

    INPUT_KEY_HOME,
    INPUT_KEY_END,
    INPUT_KEY_DELETE,
    INPUT_KEY_PAGE_UP,
    INPUT_KEY_PAGE_DOWN,
    INPUT_KEY_INSERT,
    INPUT_KEY_F2,
    INPUT_KEY_ESCAPE
} InputKey;

/*
    A snapshot of modifier state is stored with every queued key event.
    That is important because Shift/Ctrl can be released before Lua reads
    the queued character.
*/
typedef enum InputModifiers {
    INPUT_MOD_NONE  = 0,
    INPUT_MOD_SHIFT = 1 << 0,
    INPUT_MOD_CTRL  = 1 << 1,
    INPUT_MOD_ALT   = 1 << 2
} InputModifiers;

typedef struct InputEvent {
    InputEventType type;
    char ch;
    InputKey key;
    uint8_t modifiers;
} InputEvent;

/* Old helpers remain available for existing code. */
void input_push_char(char ch);
void input_push_key(InputKey key);

/* New helpers used by the VirtIO keyboard driver. */
void input_push_char_with_modifiers(char ch, uint8_t modifiers);
void input_push_key_with_modifiers(InputKey key, uint8_t modifiers);

int input_poll(InputEvent* event);

/* The platform keyboard driver implements this. */
void input_update(void);
