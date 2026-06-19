#pragma once

typedef enum InputEventType {
    INPUT_EVENT_CHAR
} InputEventType;

typedef struct InputEvent {
    InputEventType type;
    char ch;
} InputEvent;

void input_push_char(char ch);
int input_poll(InputEvent* event);

void input_update(void);