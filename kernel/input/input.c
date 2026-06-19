#include "input.h"

#define INPUT_QUEUE_SIZE 128

static InputEvent queue[INPUT_QUEUE_SIZE];

static int head = 0;
static int tail = 0;

static int input_next_index(int index) {
    return (index + 1) % INPUT_QUEUE_SIZE;
}

static int input_is_full(void) {
    return input_next_index(tail) == head;
}

static int input_is_empty(void) {
    return head == tail;
}

static void input_push_event(InputEvent event) {
    if (input_is_full()) {
        return;
    }

    queue[tail] = event;
    tail = input_next_index(tail);
}

void input_push_char(char ch) {
    InputEvent event;

    event.type = INPUT_EVENT_CHAR;
    event.ch = ch;

    input_push_event(event);
}

int input_poll(InputEvent* event) {
    if (input_is_empty()) {
        return 0;
    }

    *event = queue[head];
    head = input_next_index(head);

    return 1;
}