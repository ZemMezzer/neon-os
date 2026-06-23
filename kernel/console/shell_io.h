#pragma once

#include "neon_fs.h"

int shell_file_read_line(
    NeonFsFile* file,
    char* output,
    int output_size
);

int shell_file_write_bytes(
    NeonFsFile* file,
    const void* data,
    int size
);

int shell_file_write_text(NeonFsFile* file, const char* text);
int shell_file_write_char(NeonFsFile* file, char character);
