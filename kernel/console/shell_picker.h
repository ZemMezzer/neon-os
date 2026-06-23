#pragma once

void shell_picker_clear_result(void);

int shell_picker_set_result(const char* path);

int shell_picker_take_result(
    char* output,
    int output_size
);
