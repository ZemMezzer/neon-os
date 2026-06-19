#include "shell.h"

#include "console.h"
#include "input.h"
#include "shell_commands.h"

#define SHELL_BUFFER_SIZE 128

static char line_buffer[SHELL_BUFFER_SIZE];

static int line_length = 0;
static int line_cursor = 0;

static int prompt_x = 0;
static int prompt_y = 0;

static void shell_prompt(void) {
    console_write("> ");

    prompt_x = console_get_cursor_x();
    prompt_y = console_get_cursor_y();
}

static int shell_can_insert_char(void) {
    if (line_length >= SHELL_BUFFER_SIZE - 1) {
        return 0;
    }

    if (prompt_x + line_length >= console_get_columns() - 1) {
        return 0;
    }

    return 1;
}

static void shell_redraw_line(void) {
    console_cursor_hide();

    console_set_cursor_pos(prompt_x, prompt_y);
    console_clear_line_from_cursor();

    console_write(line_buffer);

    console_set_cursor_pos(prompt_x + line_cursor, prompt_y);

    console_cursor_show();
}

static void shell_insert_char(char c) {
    if (!shell_can_insert_char()) {
        return;
    }

    if (line_cursor == line_length) {
        line_buffer[line_length] = c;
        line_length++;
        line_cursor++;
        line_buffer[line_length] = '\0';

        console_cursor_hide();
        console_putc(c);
        console_cursor_show();

        return;
    }

    for (int i = line_length; i > line_cursor; i--) {
        line_buffer[i] = line_buffer[i - 1];
    }

    line_buffer[line_cursor] = c;

    line_length++;
    line_cursor++;

    line_buffer[line_length] = '\0';

    shell_redraw_line();
}

static void shell_backspace(void) {
    if (line_cursor <= 0) {
        return;
    }

    if (line_cursor == line_length) {
        line_cursor--;
        line_length--;
        line_buffer[line_length] = '\0';

        console_cursor_hide();
        console_backspace();
        console_cursor_show();

        return;
    }

    for (int i = line_cursor - 1; i < line_length; i++) {
        line_buffer[i] = line_buffer[i + 1];
    }

    line_cursor--;
    line_length--;

    if (line_length < 0) {
        line_length = 0;
    }

    line_buffer[line_length] = '\0';

    shell_redraw_line();
}

static void shell_delete(void) {
    if (line_cursor >= line_length) {
        return;
    }

    for (int i = line_cursor; i < line_length; i++) {
        line_buffer[i] = line_buffer[i + 1];
    }

    line_length--;

    if (line_length < 0) {
        line_length = 0;
    }

    line_buffer[line_length] = '\0';

    shell_redraw_line();
}

static void shell_enter(void) {
    console_cursor_hide();

    console_set_cursor_pos(prompt_x + line_length, prompt_y);
    console_putc('\n');

    line_buffer[line_length] = '\0';

    shell_commands_execute(line_buffer);

    line_length = 0;
    line_cursor = 0;
    line_buffer[0] = '\0';

    shell_prompt();

    console_cursor_show();
}

static void shell_move_left(void) {
    if (line_cursor <= 0) {
        return;
    }

    line_cursor--;

    console_cursor_hide();
    console_set_cursor_pos(prompt_x + line_cursor, prompt_y);
    console_cursor_show();
}

static void shell_move_right(void) {
    if (line_cursor >= line_length) {
        return;
    }

    line_cursor++;

    console_cursor_hide();
    console_set_cursor_pos(prompt_x + line_cursor, prompt_y);
    console_cursor_show();
}

static void shell_move_home(void) {
    line_cursor = 0;

    console_cursor_hide();
    console_set_cursor_pos(prompt_x + line_cursor, prompt_y);
    console_cursor_show();
}

static void shell_move_end(void) {
    line_cursor = line_length;

    console_cursor_hide();
    console_set_cursor_pos(prompt_x + line_cursor, prompt_y);
    console_cursor_show();
}

static void shell_handle_char(char c) {
    if (c == '\r' || c == '\n') {
        shell_enter();
        return;
    }

    if (c == '\b' || c == 127) {
        shell_backspace();
        return;
    }

    if (c >= 32 && c <= 126) {
        shell_insert_char(c);
        return;
    }
}

static void shell_handle_key(InputKey key) {
    if (key == INPUT_KEY_LEFT) {
        shell_move_left();
        return;
    }

    if (key == INPUT_KEY_RIGHT) {
        shell_move_right();
        return;
    }

    if (key == INPUT_KEY_HOME) {
        shell_move_home();
        return;
    }

    if (key == INPUT_KEY_END) {
        shell_move_end();
        return;
    }

    if (key == INPUT_KEY_DELETE) {
        shell_delete();
        return;
    }
}

void shell_init(void) {
    line_length = 0;
    line_cursor = 0;
    line_buffer[0] = '\0';

    shell_prompt();

    console_cursor_enable(1);
}

void shell_update(void) {
    InputEvent event;

    input_update();

    while (input_poll(&event)) {
        if (event.type == INPUT_EVENT_CHAR) {
            shell_handle_char(event.ch);
            continue;
        }

        if (event.type == INPUT_EVENT_KEY) {
            shell_handle_key(event.key);
            continue;
        }
    }

    console_cursor_update();
}