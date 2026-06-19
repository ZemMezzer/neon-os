#include "shell.h"

#include "console.h"
#include "input.h"

#define SHELL_BUFFER_SIZE 128

static char line_buffer[SHELL_BUFFER_SIZE];
static int line_length = 0;

static int str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static int str_starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text != *prefix) {
            return 0;
        }

        text++;
        prefix++;
    }

    return 1;
}

static void shell_prompt(void) {
    console_write("> ");
}

static void shell_execute(const char* command) {
    if (str_equal(command, "")) {
        return;
    }

    console_write("Unknown command: ");
    console_write(command);
    console_write("\n");
}

static void shell_handle_char(char c) {
    if (c == '\r' || c == '\n') {
        console_putc('\n');

        line_buffer[line_length] = '\0';

        shell_execute(line_buffer);

        line_length = 0;
        line_buffer[0] = '\0';

        shell_prompt();
        return;
    }

    if (c == '\b' || c == 127) {
        if (line_length > 0) {
            line_length--;
            line_buffer[line_length] = '\0';
            console_backspace();
        }

        return;
    }

    if (c >= 32 && c <= 126) {
        if (line_length < SHELL_BUFFER_SIZE - 1) {
            line_buffer[line_length] = c;
            line_length++;
            line_buffer[line_length] = '\0';

            console_putc(c);
        }

        return;
    }
}

void shell_init(void) {
    line_length = 0;
    line_buffer[0] = '\0';
    shell_prompt();
}

void shell_update(void) {
    input_update();

    InputEvent event;

    while (input_poll(&event)) {
        if (event.type == INPUT_EVENT_CHAR) {
            shell_handle_char(event.ch);
        }
    }
}