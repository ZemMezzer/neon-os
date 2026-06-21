#include <stddef.h>
#include "neon_fs.h"

#define FS_COPY_BUFFER_SIZE 512

static int string_equal(const char* left, const char* right) {
    int index = 0;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }

        index++;
    }

    return left[index] == right[index];
}

static int is_volume_root_path(const char* path) {
    return
        path != NULL &&
        path[0] >= '0' &&
        path[0] <= '9' &&
        path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\');
}

static FRESULT close_file(FIL* file, FRESULT current_result) {
    FRESULT close_result;

    if (file == NULL) {
        return current_result;
    }

    close_result = f_close(file);

    if (current_result != FR_OK) {
        return current_result;
    }

    return close_result;
}

int path_exists(const char* path) {
    FILINFO info;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    return f_stat(path, &info) == FR_OK;
}

int file_exists(const char* path) {
    FILINFO info;
    FRESULT result;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    result = f_stat(path, &info);

    return result == FR_OK && (info.fattrib & AM_DIR) == 0;
}

int directory_exists(const char* path) {
    FILINFO info;
    FRESULT result;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    result = f_stat(path, &info);

    return result == FR_OK && (info.fattrib & AM_DIR) != 0;
}

FRESULT create_file(const char* path) {
    FIL file;
    FRESULT result;

    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    result = f_open(&file, path, FA_WRITE | FA_CREATE_NEW);

    if (result != FR_OK) {
        return result;
    }

    return f_close(&file);
}

FRESULT ensure_file(const char* path) {
    FILINFO info;
    FRESULT result;

    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    result = f_stat(path, &info);

    if (result == FR_OK) {
        return (info.fattrib & AM_DIR) != 0 ? FR_EXIST : FR_OK;
    }

    if (result != FR_NO_FILE) {
        return result;
    }

    return create_file(path);
}

FRESULT write_file(const char* path, const void* data, UINT size) {
    FIL file;
    FRESULT result;
    UINT written = 0;

    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    if (size > 0 && data == NULL) {
        return FR_INVALID_PARAMETER;
    }

    result = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);

    if (result != FR_OK) {
        return result;
    }

    if (size > 0) {
        result = f_write(&file, data, size, &written);

        if (result == FR_OK && written != size) {
            result = FR_DISK_ERR;
        }
    }

    return close_file(&file, result);
}

FRESULT append_file(const char* path, const void* data, UINT size) {
    FIL file;
    FRESULT result;
    UINT written = 0;

    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    if (size > 0 && data == NULL) {
        return FR_INVALID_PARAMETER;
    }

    result = f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS);

    if (result != FR_OK) {
        return result;
    }

    result = f_lseek(&file, f_size(&file));

    if (result == FR_OK && size > 0) {
        result = f_write(&file, data, size, &written);

        if (result == FR_OK && written != size) {
            result = FR_DISK_ERR;
        }
    }

    return close_file(&file, result);
}

FRESULT create_directory(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    return f_mkdir(path);
}

FRESULT ensure_directory(const char* path) {
    FILINFO info;
    FRESULT result;

    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    result = f_stat(path, &info);

    if (result == FR_OK) {
        return (info.fattrib & AM_DIR) != 0 ? FR_OK : FR_EXIST;
    }

    if (result != FR_NO_FILE) {
        return result;
    }

    return f_mkdir(path);
}

FRESULT ensure_directory_tree(const char* absolute_path) {
    char current[PATH_MAX];
    int current_length = 3;
    int read_index = 3;

    if (!is_volume_root_path(absolute_path)) {
        return FR_INVALID_NAME;
    }

    current[0] = absolute_path[0];
    current[1] = ':';
    current[2] = '/';
    current[3] = '\0';

    while (
        absolute_path[read_index] == '/' ||
        absolute_path[read_index] == '\\'
    ) {
        read_index++;
    }

    while (absolute_path[read_index] != '\0') {
        int component_start;
        int component_length;
        FRESULT result;

        component_start = read_index;

        while (
            absolute_path[read_index] != '\0' &&
            absolute_path[read_index] != '/' &&
            absolute_path[read_index] != '\\'
        ) {
            read_index++;
        }

        component_length = read_index - component_start;

        while (
            absolute_path[read_index] == '/' ||
            absolute_path[read_index] == '\\'
        ) {
            read_index++;
        }

        if (component_length == 0) {
            continue;
        }

        if (
            component_length == 1 &&
            absolute_path[component_start] == '.'
        ) {
            return FR_INVALID_NAME;
        }

        if (
            component_length == 2 &&
            absolute_path[component_start] == '.' &&
            absolute_path[component_start + 1] == '.'
        ) {
            return FR_INVALID_NAME;
        }

        if (current_length > 3) {
            if (current_length + 1 >= PATH_MAX) {
                return FR_INVALID_NAME;
            }

            current[current_length++] = '/';
            current[current_length] = '\0';
        }

        if (current_length + component_length >= PATH_MAX) {
            return FR_INVALID_NAME;
        }

        for (int index = 0; index < component_length; index++) {
            current[current_length++] =
                absolute_path[component_start + index];
        }

        current[current_length] = '\0';

        result = ensure_directory(current);

        if (result != FR_OK) {
            return result;
        }
    }

    return FR_OK;
}

FRESULT delete_path(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    return f_unlink(path);
}

FRESULT rename_path(const char* old_path, const char* new_path) {
    if (
        old_path == NULL ||
        old_path[0] == '\0' ||
        new_path == NULL ||
        new_path[0] == '\0'
    ) {
        return FR_INVALID_NAME;
    }

    return f_rename(old_path, new_path);
}

FRESULT copy_file(const char* source_path, const char* destination_path) {
    FIL source;
    FIL destination;
    FRESULT result;
    UINT read_count;
    UINT written_count;
    unsigned char buffer[FS_COPY_BUFFER_SIZE];
    int source_opened = 0;
    int destination_opened = 0;

    if (
        source_path == NULL ||
        source_path[0] == '\0' ||
        destination_path == NULL ||
        destination_path[0] == '\0'
    ) {
        return FR_INVALID_NAME;
    }

    if (string_equal(source_path, destination_path)) {
        return FR_INVALID_NAME;
    }

    if (!file_exists(source_path)) {
        return FR_NO_FILE;
    }

    result = f_open(&source, source_path, FA_READ);

    if (result != FR_OK) {
        return result;
    }

    source_opened = 1;

    result = f_open(
        &destination,
        destination_path,
        FA_WRITE | FA_CREATE_ALWAYS
    );

    if (result != FR_OK) {
        (void)f_close(&source);
        return result;
    }

    destination_opened = 1;

    while (result == FR_OK) {
        read_count = 0;
        result = f_read(&source, buffer, sizeof(buffer), &read_count);

        if (result != FR_OK || read_count == 0) {
            break;
        }

        written_count = 0;
        result = f_write(
            &destination,
            buffer,
            read_count,
            &written_count
        );

        if (result == FR_OK && written_count != read_count) {
            result = FR_DISK_ERR;
        }
    }

    if (destination_opened) {
        result = close_file(&destination, result);
    }

    if (source_opened) {
        result = close_file(&source, result);
    }

    return result;
}

FRESULT get_file_size(const char* path, FSIZE_t* out_size) {
    FILINFO info;
    FRESULT result;

    if (
        path == NULL ||
        path[0] == '\0' ||
        out_size == NULL
    ) {
        return FR_INVALID_PARAMETER;
    }

    result = f_stat(path, &info);

    if (result != FR_OK) {
        return result;
    }

    if ((info.fattrib & AM_DIR) != 0) {
        return FR_EXIST;
    }

    *out_size = info.fsize;
    return FR_OK;
}
