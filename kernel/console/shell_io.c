#include "shell_io.h"

#include "shell_util.h"

int shell_file_read_line(
    NeonFsFile* file,
    char* output,
    int output_size
) {
    int length = 0;

    if (file == 0 || output == 0 || output_size <= 1) {
        return -1;
    }

    output[0] = '\0';

    for (;;) {
        char character;
        UINT read_count = 0;
        FRESULT result;

        result = neon_fs_file_read(file, &character, 1, &read_count);

        if (result != FR_OK) {
            return -1;
        }

        if (read_count == 0) {
            return length > 0 ? 1 : 0;
        }

        if (length + 1 >= output_size) {
            output[0] = '\0';
            return -1;
        }

        output[length++] = character;
        output[length] = '\0';

        if (character == '\n') {
            return 1;
        }
    }
}

int shell_file_write_bytes(
    NeonFsFile* file,
    const void* data,
    int size
) {
    UINT written = 0;
    FRESULT result;

    if (
        file == 0 ||
        size < 0 ||
        (size > 0 && data == 0)
    ) {
        return -1;
    }

    result = neon_fs_file_write(file, data, (UINT)size, &written);

    return result == FR_OK && written == (UINT)size ? 0 : -1;
}

int shell_file_write_text(NeonFsFile* file, const char* text) {
    if (text == 0) {
        return -1;
    }

    return shell_file_write_bytes(file, text, shell_text_length(text));
}

int shell_file_write_char(NeonFsFile* file, char character) {
    return shell_file_write_bytes(file, &character, 1);
}
