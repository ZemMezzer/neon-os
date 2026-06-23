#pragma once

int shell_text_equal(const char* left, const char* right);
int shell_text_length(const char* text);
void shell_text_copy(char* destination, int destination_size, const char* source);

int shell_text_append_char(
    char* destination,
    int destination_size,
    int* position,
    char character
);

int shell_text_append(
    char* destination,
    int destination_size,
    int* position,
    const char* text
);

char shell_text_ascii_lower(char character);

void shell_text_trim_line(char* text);
void shell_text_trim_script_line(char* text);

void shell_print_error(const char* text);
