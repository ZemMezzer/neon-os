#include "input.h"

#define INPUT_QUEUE_SIZE 128

static volatile InputEvent queue[INPUT_QUEUE_SIZE];

static volatile int head = 0;
static volatile int tail = 0;

static int input_next_index(int index) {
    return (index + 1) % INPUT_QUEUE_SIZE;
}

static int input_is_full(void) {
    int current_tail = tail;
    int current_head = head;

    return input_next_index(current_tail) == current_head;
}

static int input_is_empty(void) {
    return head == tail;
}

static void input_push_raw(InputEventType type, char ch, InputKey key) {
    int current_tail;
    int next_tail;

    if (input_is_full()) {
        return;
    }

    current_tail = tail;
    next_tail = input_next_index(current_tail);

    queue[current_tail].type = type;
    queue[current_tail].ch = ch;
    queue[current_tail].key = key;

    tail = next_tail;
}

void input_push_char(char ch) {
    input_push_raw(INPUT_EVENT_CHAR, ch, INPUT_KEY_NONE);
}

void input_push_key(InputKey key) {
    input_push_raw(INPUT_EVENT_KEY, 0, key);
}

int input_poll(InputEvent* event) {
    int current_head;

    if (!event) {
        return 0;
    }

    if (input_is_empty()) {
        return 0;
    }

    current_head = head;

    event->type = queue[current_head].type;
    event->ch = queue[current_head].ch;
    event->key = queue[current_head].key;

    head = input_next_index(current_head);

    return 1;
}