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

    INPUT_KEY_F1,
    INPUT_KEY_F2,
    INPUT_KEY_F3,
    INPUT_KEY_F4,
    INPUT_KEY_F5,
    INPUT_KEY_F6,
    INPUT_KEY_F7,
    INPUT_KEY_F8,
    INPUT_KEY_F9,
    INPUT_KEY_F10,
    INPUT_KEY_F11,
    INPUT_KEY_F12,

    INPUT_KEY_ESCAPE
} InputKey;

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

void input_push_char(char ch);
void input_push_key(InputKey key);

void input_push_char_with_modifiers(char ch, uint8_t modifiers);
void input_push_key_with_modifiers(InputKey key, uint8_t modifiers);

int input_poll(InputEvent* event);

int input_take_global_close_request(void);

void input_update(void);
