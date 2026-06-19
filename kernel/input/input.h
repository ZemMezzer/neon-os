#pragma once

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
    INPUT_KEY_DELETE
} InputKey;

typedef struct InputEvent {
    InputEventType type;
    char ch;
    InputKey key;
} InputEvent;

void input_push_char(char ch);
void input_push_key(InputKey key);

int input_poll(InputEvent* event);

void input_update(void);