#include "shell_util.h"

#include "console.h"

int shell_text_equal(const char* left, const char* right) {
    if (left == 0 || right == 0) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }

        left++;
        right++;
    }

    return *left == *right;
}

int shell_text_length(const char* text) {
    int length = 0;

    while (text != 0 && text[length] != '\0') {
        length++;
    }

    return length;
}

void shell_text_copy(
    char* destination,
    int destination_size,
    const char* source
) {
    int index = 0;

    if (destination == 0 || destination_size <= 0) {
        return;
    }

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    while (source[index] != '\0' && index + 1 < destination_size) {
        destination[index] = source[index];
        index++;
    }

    destination[index] = '\0';
}

int shell_text_append_char(
    char* destination,
    int destination_size,
    int* position,
    char character
) {
    if (
        destination == 0 ||
        position == 0 ||
        *position < 0 ||
        *position + 1 >= destination_size
    ) {
        return -1;
    }

    destination[*position] = character;
    *position = *position + 1;
    destination[*position] = '\0';
    return 0;
}

int shell_text_append(
    char* destination,
    int destination_size,
    int* position,
    const char* text
) {
    int index = 0;

    if (text == 0) {
        return 0;
    }

    while (text[index] != '\0') {
        if (
            shell_text_append_char(
                destination,
                destination_size,
                position,
                text[index]
            ) != 0
        ) {
            return -1;
        }

        index++;
    }

    return 0;
}

char shell_text_ascii_lower(char character) {
    if (character >= 'A' && character <= 'Z') {
        return (char)(character - 'A' + 'a');
    }

    return character;
}

void shell_text_trim_line(char* text) {
    int start = 0;
    int end;
    int write_index;

    if (text == 0) {
        return;
    }

    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }

    end = shell_text_length(text);

    while (
        end > start &&
        (
            text[end - 1] == ' ' ||
            text[end - 1] == '\t' ||
            text[end - 1] == '\r' ||
            text[end - 1] == '\n'
        )
    ) {
        end--;
    }

    if (start > 0) {
        for (write_index = 0; write_index < end - start; write_index++) {
            text[write_index] = text[start + write_index];
        }

        end -= start;
    }

    text[end] = '\0';
}

void shell_text_trim_script_line(char* text) {
    int start = 0;
    int end;
    int write_index;

    if (text == 0) {
        return;
    }

    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }

    end = shell_text_length(text);

    while (
        end > start &&
        (text[end - 1] == '\r' || text[end - 1] == '\n')
    ) {
        end--;
    }

    if (start > 0) {
        for (write_index = 0; write_index < end - start; write_index++) {
            text[write_index] = text[start + write_index];
        }

        end -= start;
    }

    text[end] = '\0';
}

void shell_print_error(const char* text) {
    console_write("Error: ");
    console_write(text != 0 ? text : "unknown error");
    console_write("\n");
}
