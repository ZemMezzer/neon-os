#include <stddef.h>

#include "neon_fs.h"

#define FS_COPY_BUFFER_SIZE 512
#define FS_MAX_RECURSION 24


static size_t string_length(const char* text) {
    size_t length = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}


static int is_separator(char character) {
    return character == '/' || character == '\\';
}


static int is_dot_entry(const char* name) {
    if (name == NULL) {
        return 0;
    }

    return
        (name[0] == '.' && name[1] == '\0') ||
        (name[0] == '.' && name[1] == '.' && name[2] == '\0');
}


static int string_equal(const char* left, const char* right) {
    size_t index = 0;

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


static int paths_equal(const char* left, const char* right) {
    size_t left_length;
    size_t right_length;
    size_t index;

    if (left == NULL || right == NULL) {
        return 0;
    }

    left_length = string_length(left);
    right_length = string_length(right);

    while (left_length > 1 && is_separator(left[left_length - 1])) {
        left_length--;
    }

    while (right_length > 1 && is_separator(right[right_length - 1])) {
        right_length--;
    }

    if (left_length != right_length) {
        return 0;
    }

    for (index = 0; index < left_length; index++) {
        char a = left[index];
        char b = right[index];

        if (is_separator(a)) {
            a = '/';
        }

        if (is_separator(b)) {
            b = '/';
        }

        if (a != b) {
            return 0;
        }
    }

    return 1;
}


static int path_is_child_of(const char* parent, const char* child) {
    size_t parent_length;
    size_t child_length;
    size_t index;

    if (parent == NULL || child == NULL) {
        return 0;
    }

    parent_length = string_length(parent);
    child_length = string_length(child);

    if (
        parent_length == 1 &&
        is_separator(parent[0]) &&
        child_length > 1 &&
        is_separator(child[0])
    ) {
        return 1;
    }

    while (parent_length > 1 && is_separator(parent[parent_length - 1])) {
        parent_length--;
    }

    while (child_length > 1 && is_separator(child[child_length - 1])) {
        child_length--;
    }

    if (child_length <= parent_length) {
        return 0;
    }

    for (index = 0; index < parent_length; index++) {
        char a = parent[index];
        char b = child[index];

        if (is_separator(a)) {
            a = '/';
        }

        if (is_separator(b)) {
            b = '/';
        }

        if (a != b) {
            return 0;
        }
    }

    return is_separator(child[parent_length]);
}


static int join_path(
    char* output,
    size_t output_capacity,
    const char* parent,
    const char* name
) {
    size_t parent_length;
    size_t name_length;
    size_t position;
    int needs_separator;
    size_t index;

    if (output == NULL || output_capacity == 0 || parent == NULL || name == NULL) {
        return 0;
    }

    parent_length = string_length(parent);
    name_length = string_length(name);

    while (name_length > 0 && is_separator(name[0])) {
        name++;
        name_length--;
    }

    needs_separator =
        parent_length > 0 &&
        !is_separator(parent[parent_length - 1]) &&
        parent[parent_length - 1] != ':';

    if (
        parent_length +
        (needs_separator ? 1U : 0U) +
        name_length +
        1U >
        output_capacity
    ) {
        return 0;
    }

    position = 0;

    for (index = 0; index < parent_length; index++) {
        output[position++] = parent[index] == '\\' ? '/' : parent[index];
    }

    if (needs_separator) {
        output[position++] = '/';
    }

    for (index = 0; index < name_length; index++) {
        output[position++] = name[index] == '\\' ? '/' : name[index];
    }

    output[position] = '\0';
    return 1;
}


static int is_volume_root_path(const char* path) {
    return
        path != NULL &&
        path[0] >= '0' &&
        path[0] <= '9' &&
        path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\');
}


static FRESULT close_fatfs_file(FIL* file, FRESULT current_result) {
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


FRESULT neon_fs_file_open(
    NeonFsFile* file,
    const char* path,
    NeonFsFileOpenMode mode
) {
    FRESULT result;
    BYTE flags;

    if (file == NULL || path == NULL || path[0] == '\0') {
        return FR_INVALID_PARAMETER;
    }

    switch (mode) {
        case NEON_FS_FILE_OPEN_READ:
            flags = FA_READ | FA_OPEN_EXISTING;
            break;

        case NEON_FS_FILE_OPEN_WRITE_TRUNCATE:
            flags = FA_WRITE | FA_CREATE_ALWAYS;
            break;

        case NEON_FS_FILE_OPEN_WRITE_APPEND:
            flags = FA_WRITE | FA_OPEN_ALWAYS;
            break;

        case NEON_FS_FILE_OPEN_WRITE_NEW:
            flags = FA_WRITE | FA_CREATE_NEW;
            break;

        default:
            return FR_INVALID_PARAMETER;
    }

    result = f_open(&file->handle, path, flags);
    if (result != FR_OK) {
        return result;
    }

    if (mode == NEON_FS_FILE_OPEN_WRITE_APPEND) {
        result = f_lseek(&file->handle, f_size(&file->handle));
        if (result != FR_OK) {
            (void)f_close(&file->handle);
            return result;
        }
    }

    return FR_OK;
}


FRESULT neon_fs_file_read(
    NeonFsFile* file,
    void* buffer,
    UINT buffer_size,
    UINT* out_read
) {
    if (
        file == NULL ||
        out_read == NULL ||
        (buffer_size > 0 && buffer == NULL)
    ) {
        return FR_INVALID_PARAMETER;
    }

    return f_read(&file->handle, buffer, buffer_size, out_read);
}


FRESULT neon_fs_file_write(
    NeonFsFile* file,
    const void* data,
    UINT data_size,
    UINT* out_written
) {
    if (
        file == NULL ||
        out_written == NULL ||
        (data_size > 0 && data == NULL)
    ) {
        return FR_INVALID_PARAMETER;
    }

    return f_write(&file->handle, data, data_size, out_written);
}


FRESULT neon_fs_file_close(NeonFsFile* file) {
    if (file == NULL) {
        return FR_INVALID_PARAMETER;
    }

    return f_close(&file->handle);
}


static void copy_info(NeonFsEntry* destination, const FILINFO* source) {
    size_t index = 0;

    destination->size = source->fsize;
    destination->attributes = source->fattrib;

    while (
        source->fname[index] != '\0' &&
        index + 1U < sizeof(destination->name)
    ) {
        destination->name[index] = source->fname[index];
        index++;
    }

    destination->name[index] = '\0';
}


static FRESULT delete_tree_internal(const char* path, int depth);


static FRESULT copy_path_internal(
    const char* source_path,
    const char* destination_path,
    int depth
) {
    NeonFsEntry source_info;
    NeonFsDirectory directory;
    FRESULT result;
    int directory_opened = 0;

    if (depth > FS_MAX_RECURSION) {
        return FR_NOT_ENOUGH_CORE;
    }

    result = get_path_info(source_path, &source_info);
    if (result != FR_OK) {
        return result;
    }

    if ((source_info.attributes & AM_DIR) == 0) {
        return copy_file(source_path, destination_path);
    }

    result = create_directory(destination_path);
    if (result != FR_OK) {
        return result;
    }

    result = open_directory(&directory, source_path);
    if (result != FR_OK) {
        (void)delete_path(destination_path);
        return result;
    }

    directory_opened = 1;

    for (;;) {
        NeonFsEntry child_info;
        int end = 0;
        char child_source[NEON_FS_PATH_MAX];
        char child_destination[NEON_FS_PATH_MAX];

        result = read_directory(&directory, &child_info, &end);
        if (result != FR_OK || end) {
            break;
        }

        if (is_dot_entry(child_info.name)) {
            continue;
        }

        if (
            !join_path(
                child_source,
                sizeof(child_source),
                source_path,
                child_info.name
            ) ||
            !join_path(
                child_destination,
                sizeof(child_destination),
                destination_path,
                child_info.name
            )
        ) {
            result = FR_INVALID_NAME;
            break;
        }

        result = copy_path_internal(
            child_source,
            child_destination,
            depth + 1
        );

        if (result != FR_OK) {
            break;
        }
    }

    if (directory_opened) {
        FRESULT close_result = close_directory(&directory);

        if (result == FR_OK && close_result != FR_OK) {
            result = close_result;
        }
    }

    if (result != FR_OK) {
        (void)delete_tree_internal(destination_path, 0);
    }

    return result;
}


static FRESULT delete_tree_internal(const char* path, int depth) {
    NeonFsEntry info;
    FRESULT result;

    if (depth > FS_MAX_RECURSION) {
        return FR_NOT_ENOUGH_CORE;
    }

    result = get_path_info(path, &info);
    if (result != FR_OK) {
        return result;
    }

    if ((info.attributes & AM_DIR) == 0) {
        return delete_path(path);
    }

    {
        NeonFsDirectory directory;

        result = open_directory(&directory, path);
        if (result != FR_OK) {
            return result;
        }

        for (;;) {
            NeonFsEntry child_info;
            int end = 0;
            char child_path[NEON_FS_PATH_MAX];

            result = read_directory(&directory, &child_info, &end);
            if (result != FR_OK || end) {
                break;
            }

            if (is_dot_entry(child_info.name)) {
                continue;
            }

            if (!join_path(
                child_path,
                sizeof(child_path),
                path,
                child_info.name
            )) {
                result = FR_INVALID_NAME;
                break;
            }

            result = delete_tree_internal(child_path, depth + 1);
            if (result != FR_OK) {
                break;
            }
        }

        {
            FRESULT close_result = close_directory(&directory);

            if (result == FR_OK && close_result != FR_OK) {
                result = close_result;
            }
        }
    }

    if (result != FR_OK) {
        return result;
    }

    return delete_path(path);
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

    return close_fatfs_file(&file, result);
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

    return close_fatfs_file(&file, result);
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
    char current[NEON_FS_PATH_MAX];
    size_t current_length = 3;
    size_t read_index = 3;

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
        size_t component_start;
        size_t component_length;
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
            if (current_length + 1U >= sizeof(current)) {
                return FR_INVALID_NAME;
            }

            current[current_length++] = '/';
            current[current_length] = '\0';
        }

        if (current_length + component_length >= sizeof(current)) {
            return FR_INVALID_NAME;
        }

        {
            size_t index;

            for (index = 0; index < component_length; index++) {
                current[current_length++] =
                    absolute_path[component_start + index];
            }
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


FRESULT delete_tree(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return FR_INVALID_NAME;
    }

    return delete_tree_internal(path, 0);
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
        result = close_fatfs_file(&destination, result);
    }

    if (source_opened) {
        result = close_fatfs_file(&source, result);
    }

    return result;
}


FRESULT copy_path(const char* source_path, const char* destination_path) {
    if (
        source_path == NULL ||
        source_path[0] == '\0' ||
        destination_path == NULL ||
        destination_path[0] == '\0'
    ) {
        return FR_INVALID_NAME;
    }

    if (paths_equal(source_path, destination_path)) {
        return FR_INVALID_NAME;
    }

    if (path_is_child_of(source_path, destination_path)) {
        return FR_INVALID_NAME;
    }

    if (path_exists(destination_path)) {
        return FR_EXIST;
    }

    return copy_path_internal(source_path, destination_path, 0);
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


FRESULT get_path_info(const char* path, NeonFsEntry* out_info) {
    FILINFO info;
    FRESULT result;

    if (
        path == NULL ||
        path[0] == '\0' ||
        out_info == NULL
    ) {
        return FR_INVALID_PARAMETER;
    }

    result = f_stat(path, &info);
    if (result != FR_OK) {
        return result;
    }

    copy_info(out_info, &info);
    return FR_OK;
}


FRESULT open_directory(NeonFsDirectory* directory, const char* path) {
    if (directory == NULL || path == NULL) {
        return FR_INVALID_PARAMETER;
    }

    return f_opendir(&directory->handle, path);
}


FRESULT read_directory(
    NeonFsDirectory* directory,
    NeonFsEntry* out_entry,
    int* out_end
) {
    FILINFO info;
    FRESULT result;

    if (directory == NULL || out_entry == NULL || out_end == NULL) {
        return FR_INVALID_PARAMETER;
    }

    result = f_readdir(&directory->handle, &info);
    if (result != FR_OK) {
        return result;
    }

    if (info.fname[0] == '\0') {
        out_entry->name[0] = '\0';
        out_entry->size = 0;
        out_entry->attributes = 0;
        *out_end = 1;
        return FR_OK;
    }

    copy_info(out_entry, &info);
    *out_end = 0;
    return FR_OK;
}


FRESULT close_directory(NeonFsDirectory* directory) {
    if (directory == NULL) {
        return FR_INVALID_PARAMETER;
    }

    return f_closedir(&directory->handle);
}


FRESULT get_free_space(const char* path, uint64_t* out_bytes) {
    DWORD free_clusters;
    FATFS* filesystem;
    FRESULT result;

    if (path == NULL || out_bytes == NULL) {
        return FR_INVALID_PARAMETER;
    }

    free_clusters = 0;
    filesystem = NULL;

    result = f_getfree(path, &free_clusters, &filesystem);
    if (result != FR_OK) {
        return result;
    }

    if (filesystem == NULL) {
        return FR_INT_ERR;
    }

    *out_bytes =
        (uint64_t)free_clusters *
        (uint64_t)filesystem->csize *
        (uint64_t)FF_MAX_SS;

    return FR_OK;
}


FRESULT get_capacity(const char* path, uint64_t* out_bytes) {
    DWORD free_clusters;
    FATFS* filesystem;
    FRESULT result;

    if (path == NULL || out_bytes == NULL) {
        return FR_INVALID_PARAMETER;
    }

    free_clusters = 0;
    filesystem = NULL;

    result = f_getfree(path, &free_clusters, &filesystem);
    if (result != FR_OK) {
        return result;
    }

    if (filesystem == NULL) {
        return FR_INT_ERR;
    }

    *out_bytes =
        (uint64_t)(filesystem->n_fatent - 2U) *
        (uint64_t)filesystem->csize *
        (uint64_t)FF_MAX_SS;

    return FR_OK;
}
