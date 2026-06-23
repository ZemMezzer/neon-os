#include "shell_path.h"

#include "shell_io.h"
#include "shell_limits.h"
#include "shell_util.h"

#include "neon_fs.h"

#define SHELL_PATH_FILE "0:/.system/variables/PATH.txt"

typedef struct ShellPathList {
    char raw[SHELL_PATH_MAX_ENTRIES][SHELL_PATH_MAX];
    char absolute[SHELL_PATH_MAX_ENTRIES][SHELL_PATH_MAX];
    int count;
} ShellPathList;

static char shell_cwd[SHELL_PATH_MAX] = "0:/";

static int shell_path_is_volume_path(const char* path) {
    return
        path != 0 &&
        path[0] >= '0' &&
        path[0] <= '9' &&
        path[1] == ':';
}

static int shell_path_normalize_absolute(
    const char* input,
    char* output,
    int output_size
) {
    int read_index;
    int write_index;
    int component_restore[SHELL_PATH_MAX / 2];
    int component_count = 0;

    if (
        input == 0 ||
        output == 0 ||
        output_size < 4 ||
        !shell_path_is_volume_path(input)
    ) {
        return -1;
    }

    output[0] = input[0];
    output[1] = ':';
    output[2] = '/';
    output[3] = '\0';

    write_index = 3;
    read_index = 2;

    while (input[read_index] == '/' || input[read_index] == '\\') {
        read_index++;
    }

    while (input[read_index] != '\0') {
        int component_start = read_index;
        int component_length;
        int restore_index;
        int component_index;

        while (
            input[read_index] != '\0' &&
            input[read_index] != '/' &&
            input[read_index] != '\\'
        ) {
            read_index++;
        }

        component_length = read_index - component_start;

        while (input[read_index] == '/' || input[read_index] == '\\') {
            read_index++;
        }

        if (
            component_length == 0 ||
            (component_length == 1 && input[component_start] == '.')
        ) {
            continue;
        }

        if (
            component_length == 2 &&
            input[component_start] == '.' &&
            input[component_start + 1] == '.'
        ) {
            if (component_count > 0) {
                component_count--;
                write_index = component_restore[component_count];
                output[write_index] = '\0';
            }

            continue;
        }

        if (
            component_count >=
            (int)(sizeof(component_restore) / sizeof(component_restore[0]))
        ) {
            return -1;
        }

        restore_index = write_index;

        if (
            write_index > 3 &&
            shell_text_append_char(
                output,
                output_size,
                &write_index,
                '/'
            ) != 0
        ) {
            return -1;
        }

        for (
            component_index = 0;
            component_index < component_length;
            component_index++
        ) {
            if (
                shell_text_append_char(
                    output,
                    output_size,
                    &write_index,
                    input[component_start + component_index]
                ) != 0
            ) {
                return -1;
            }
        }

        component_restore[component_count] = restore_index;
        component_count++;
    }

    return 0;
}

int shell_resolve_path(
    const char* input,
    char* output,
    int output_size
) {
    char raw[SHELL_PATH_MAX];
    int raw_position = 0;

    if (input == 0 || output == 0 || output_size <= 0) {
        return -1;
    }

    raw[0] = '\0';

    if (input[0] == '\0') {
        shell_text_copy(raw, sizeof(raw), shell_cwd);
    } else if (shell_path_is_volume_path(input)) {
        shell_text_copy(raw, sizeof(raw), input);
    } else if (input[0] == '/' || input[0] == '\\') {
        if (
            shell_text_append_char(raw, sizeof(raw), &raw_position, '0') != 0 ||
            shell_text_append_char(raw, sizeof(raw), &raw_position, ':') != 0 ||
            shell_text_append(raw, sizeof(raw), &raw_position, input) != 0
        ) {
            return -1;
        }
    } else {
        if (
            shell_text_append(raw, sizeof(raw), &raw_position, shell_cwd) != 0
        ) {
            return -1;
        }

        if (
            raw_position > 0 &&
            raw[raw_position - 1] != '/' &&
            shell_text_append_char(raw, sizeof(raw), &raw_position, '/') != 0
        ) {
            return -1;
        }

        if (
            shell_text_append(raw, sizeof(raw), &raw_position, input) != 0
        ) {
            return -1;
        }
    }

    return shell_path_normalize_absolute(raw, output, output_size);
}

int shell_get_current_directory(
    char* output,
    int output_size
) {
    if (output == 0 || output_size <= 0) {
        return -1;
    }

    if (shell_text_length(shell_cwd) >= output_size) {
        output[0] = '\0';
        return -1;
    }

    shell_text_copy(output, output_size, shell_cwd);
    return 0;
}

int shell_set_current_directory(const char* input) {
    char path[SHELL_PATH_MAX];
    NeonFsDirectory directory;
    FRESULT result;

    if (input == 0 || input[0] == '\0') {
        return -1;
    }

    if (shell_resolve_path(input, path, sizeof(path)) != 0) {
        return -1;
    }

    result = open_directory(&directory, path);

    if (result != FR_OK) {
        return -1;
    }

    if (close_directory(&directory) != FR_OK) {
        return -1;
    }

    shell_text_copy(shell_cwd, sizeof(shell_cwd), path);
    return 0;
}

void shell_path_parent(char* absolute_path) {
    int length;
    int index;

    if (
        absolute_path == 0 ||
        shell_text_equal(absolute_path, "0:/")
    ) {
        return;
    }

    length = shell_text_length(absolute_path);

    for (index = length - 1; index >= 0; index--) {
        if (absolute_path[index] == '/') {
            if (index <= 2) {
                absolute_path[0] = '0';
                absolute_path[1] = ':';
                absolute_path[2] = '/';
                absolute_path[3] = '\0';
            } else {
                absolute_path[index] = '\0';
            }

            return;
        }
    }
}

int shell_path_is_directory(const char* path) {
    return directory_exists(path);
}

int shell_path_is_regular_file(const char* path) {
    return file_exists(path);
}

static int shell_path_entry_to_absolute(
    const char* entry,
    char* output,
    int output_size
) {
    char raw[SHELL_PATH_MAX];
    int raw_position = 0;
    int entry_index = 0;

    if (
        entry == 0 ||
        output == 0 ||
        output_size <= 0 ||
        entry[0] == '\0'
    ) {
        return -1;
    }

    raw[0] = '\0';

    if (shell_path_is_volume_path(entry)) {
        shell_text_copy(raw, sizeof(raw), entry);
    } else {
        if (
            shell_text_append_char(raw, sizeof(raw), &raw_position, '0') != 0 ||
            shell_text_append_char(raw, sizeof(raw), &raw_position, ':') != 0 ||
            shell_text_append_char(raw, sizeof(raw), &raw_position, '/') != 0
        ) {
            return -1;
        }

        if (
            entry[0] == '.' &&
            (entry[1] == '/' || entry[1] == '\\')
        ) {
            entry_index = 2;
        } else if (entry[0] == '/' || entry[0] == '\\') {
            entry_index = 1;
        }

        if (
            shell_text_append(
                raw,
                sizeof(raw),
                &raw_position,
                entry + entry_index
            ) != 0
        ) {
            return -1;
        }
    }

    return shell_path_normalize_absolute(raw, output, output_size);
}

static int shell_path_absolute_to_entry(
    const char* absolute,
    char* output,
    int output_size
) {
    int position = 0;
    int index;

    if (
        absolute == 0 ||
        output == 0 ||
        output_size <= 0 ||
        !shell_path_is_volume_path(absolute)
    ) {
        return -1;
    }

    output[0] = '\0';

    if (absolute[2] == '/') {
        if (
            shell_text_append_char(output, output_size, &position, '.') != 0 ||
            shell_text_append_char(output, output_size, &position, '/') != 0
        ) {
            return -1;
        }

        for (index = 3; absolute[index] != '\0'; index++) {
            if (
                shell_text_append_char(
                    output,
                    output_size,
                    &position,
                    absolute[index]
                ) != 0
            ) {
                return -1;
            }
        }

        return 0;
    }

    shell_text_copy(output, output_size, absolute);
    return 0;
}

static int shell_path_join(
    const char* directory,
    const char* name,
    char* output,
    int output_size
) {
    int position = 0;

    if (
        directory == 0 ||
        name == 0 ||
        output == 0 ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = '\0';

    if (
        shell_text_append(output, output_size, &position, directory) != 0
    ) {
        return -1;
    }

    if (
        position > 0 &&
        output[position - 1] != '/' &&
        shell_text_append_char(output, output_size, &position, '/') != 0
    ) {
        return -1;
    }

    return shell_text_append(output, output_size, &position, name);
}

static void shell_path_trim_line(char* text) {
    shell_text_trim_line(text);
}

static int shell_path_load_list(ShellPathList* list) {
    NeonFsFile file;
    char line[SHELL_PATH_MAX];
    FRESULT result;

    if (list == 0) {
        return -1;
    }

    list->count = 0;

    result = neon_fs_file_open(
        &file,
        SHELL_PATH_FILE,
        NEON_FS_FILE_OPEN_READ
    );

    if (result != FR_OK) {
        return -1;
    }

    for (;;) {
        int line_result = shell_file_read_line(&file, line, sizeof(line));

        if (line_result == 0) {
            break;
        }

        if (line_result < 0) {
            (void)neon_fs_file_close(&file);
            return -1;
        }

        shell_path_trim_line(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (list->count >= SHELL_PATH_MAX_ENTRIES) {
            (void)neon_fs_file_close(&file);
            return -1;
        }

        if (
            shell_path_entry_to_absolute(
                line,
                list->absolute[list->count],
                sizeof(list->absolute[list->count])
            ) != 0
        ) {
            (void)neon_fs_file_close(&file);
            return -1;
        }

        shell_text_copy(
            list->raw[list->count],
            sizeof(list->raw[list->count]),
            line
        );

        list->count++;
    }

    return neon_fs_file_close(&file) == FR_OK ? 0 : -1;
}

static int shell_path_write_list(const ShellPathList* list) {
    NeonFsFile file;
    FRESULT result;
    int index;

    if (list == 0) {
        return -1;
    }

    result = neon_fs_file_open(
        &file,
        SHELL_PATH_FILE,
        NEON_FS_FILE_OPEN_WRITE_TRUNCATE
    );

    if (result != FR_OK) {
        return -1;
    }

    for (index = 0; index < list->count; index++) {
        if (
            shell_file_write_text(&file, list->raw[index]) != 0 ||
            shell_file_write_char(&file, '\n') != 0
        ) {
            (void)neon_fs_file_close(&file);
            return -1;
        }
    }

    return neon_fs_file_close(&file) == FR_OK ? 0 : -1;
}

static int shell_path_equal_case_insensitive(
    const char* left,
    const char* right
) {
    int index = 0;

    if (left == 0 || right == 0) {
        return 0;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        if (
            shell_text_ascii_lower(left[index]) !=
            shell_text_ascii_lower(right[index])
        ) {
            return 0;
        }

        index++;
    }

    return left[index] == right[index];
}

static int shell_path_is_direct_child(
    const char* parent,
    const char* child
) {
    int parent_length;
    int index;
    int child_name_start;

    if (parent == 0 || child == 0) {
        return 0;
    }

    parent_length = shell_text_length(parent);

    if (parent_length <= 0) {
        return 0;
    }

    for (index = 0; index < parent_length; index++) {
        if (
            child[index] == '\0' ||
            shell_text_ascii_lower(parent[index]) !=
            shell_text_ascii_lower(child[index])
        ) {
            return 0;
        }
    }

    child_name_start = parent_length;

    if (parent[parent_length - 1] != '/') {
        if (child[child_name_start] != '/') {
            return 0;
        }

        child_name_start++;
    }

    if (child[child_name_start] == '\0') {
        return 0;
    }

    if (
        (child[child_name_start] == '.' &&
         child[child_name_start + 1] == '\0') ||
        (child[child_name_start] == '.' &&
         child[child_name_start + 1] == '.' &&
         child[child_name_start + 2] == '\0')
    ) {
        return 0;
    }

    for (index = child_name_start; child[index] != '\0'; index++) {
        if (child[index] == '/' || child[index] == '\\') {
            return 0;
        }
    }

    return 1;
}

static int shell_path_name_has_path_syntax(const char* name) {
    int index = 0;

    if (name == 0) {
        return 0;
    }

    while (name[index] != '\0') {
        if (
            name[index] == '/' ||
            name[index] == '\\' ||
            name[index] == ':'
        ) {
            return 1;
        }

        index++;
    }

    return 0;
}

int shell_visit_path_directories(
    ShellPathDirectoryVisitor visitor,
    void* user_data
) {
    ShellPathList paths;
    int index;

    if (visitor == 0) {
        return -1;
    }

    if (shell_path_load_list(&paths) != 0) {
        return -1;
    }

    for (index = 0; index < paths.count; index++) {
        int earlier_index;
        int duplicate = 0;
        int visitor_result;

        if (!shell_path_is_directory(paths.absolute[index])) {
            continue;
        }

        for (earlier_index = 0; earlier_index < index; earlier_index++) {
            if (
                shell_path_is_directory(paths.absolute[earlier_index]) &&
                shell_path_equal_case_insensitive(
                    paths.absolute[earlier_index],
                    paths.absolute[index]
                )
            ) {
                duplicate = 1;
                break;
            }
        }

        if (duplicate) {
            continue;
        }

        visitor_result = visitor(
            paths.absolute[index],
            index,
            user_data
        );

        if (visitor_result != 0) {
            return visitor_result;
        }
    }

    return 0;
}

int shell_path_visit_entries(
    ShellPathEntryVisitor visitor,
    void* user_data
) {
    ShellPathList paths;
    int index;

    if (visitor == 0 || shell_path_load_list(&paths) != 0) {
        return -1;
    }

    for (index = 0; index < paths.count; index++) {
        int result = visitor(
            paths.raw[index],
            paths.absolute[index],
            index,
            user_data
        );

        if (result != 0) {
            return result;
        }
    }

    return 0;
}

int shell_is_direct_child_of_path(const char* absolute_path) {
    ShellPathList paths;
    char normalized_path[SHELL_PATH_MAX];
    int index;

    if (absolute_path == 0 || absolute_path[0] == '\0') {
        return -1;
    }

    if (
        shell_resolve_path(
            absolute_path,
            normalized_path,
            sizeof(normalized_path)
        ) != 0
    ) {
        return -1;
    }

    if (shell_path_load_list(&paths) != 0) {
        return -1;
    }

    for (index = 0; index < paths.count; index++) {
        if (!shell_path_is_directory(paths.absolute[index])) {
            continue;
        }

        if (
            shell_path_is_direct_child(
                paths.absolute[index],
                normalized_path
            )
        ) {
            return 1;
        }
    }

    return 0;
}

int shell_find_path(
    const char* name,
    char* output,
    int output_size
) {
    ShellPathList paths;
    char candidate[SHELL_PATH_MAX];
    int index;

    if (
        name == 0 ||
        name[0] == '\0' ||
        output == 0 ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = '\0';

    if (shell_path_name_has_path_syntax(name)) {
        if (shell_resolve_path(name, candidate, sizeof(candidate)) != 0) {
            return -1;
        }

        if (path_exists(candidate)) {
            shell_text_copy(output, output_size, candidate);
            return 1;
        }

        return 0;
    }

    if (shell_resolve_path(name, candidate, sizeof(candidate)) != 0) {
        return -1;
    }

    if (path_exists(candidate)) {
        shell_text_copy(output, output_size, candidate);
        return 1;
    }

    if (shell_path_load_list(&paths) != 0) {
        return -1;
    }

    for (index = 0; index < paths.count; index++) {
        if (!shell_path_is_directory(paths.absolute[index])) {
            continue;
        }

        if (
            shell_path_join(
                paths.absolute[index],
                name,
                candidate,
                sizeof(candidate)
            ) != 0
        ) {
            continue;
        }

        if (path_exists(candidate)) {
            shell_text_copy(output, output_size, candidate);
            return 1;
        }
    }

    return 0;
}

int shell_find_command_file(
    const char* filename,
    char* output,
    int output_size
) {
    ShellPathList paths;
    char candidate[SHELL_PATH_MAX];
    int index;

    if (
        filename == 0 ||
        filename[0] == '\0' ||
        output == 0 ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = '\0';

    if (shell_path_name_has_path_syntax(filename)) {
        if (
            shell_resolve_path(filename, candidate, sizeof(candidate)) != 0
        ) {
            return -1;
        }

        if (shell_path_is_regular_file(candidate)) {
            shell_text_copy(output, output_size, candidate);
            return 1;
        }

        return 0;
    }

    if (shell_resolve_path(filename, candidate, sizeof(candidate)) != 0) {
        return -1;
    }

    if (shell_path_is_regular_file(candidate)) {
        shell_text_copy(output, output_size, candidate);
        return 1;
    }

    if (shell_path_load_list(&paths) != 0) {
        return -1;
    }

    for (index = 0; index < paths.count; index++) {
        if (!shell_path_is_directory(paths.absolute[index])) {
            continue;
        }

        if (
            shell_path_join(
                paths.absolute[index],
                filename,
                candidate,
                sizeof(candidate)
            ) != 0
        ) {
            continue;
        }

        if (shell_path_is_regular_file(candidate)) {
            shell_text_copy(output, output_size, candidate);
            return 1;
        }
    }

    return 0;
}

ShellPathStatus shell_path_add_directory(
    const char* input,
    char* stored_entry,
    int stored_entry_size
) {
    ShellPathList paths;
    char absolute[SHELL_PATH_MAX];
    char stored[SHELL_PATH_MAX];
    int index;

    if (stored_entry != 0 && stored_entry_size > 0) {
        stored_entry[0] = '\0';
    }

    if (input == 0 || input[0] == '\0') {
        return SHELL_PATH_STATUS_INVALID;
    }

    if (shell_path_load_list(&paths) != 0) {
        return SHELL_PATH_STATUS_IO_ERROR;
    }

    if (
        shell_resolve_path(input, absolute, sizeof(absolute)) != 0
    ) {
        return SHELL_PATH_STATUS_INVALID;
    }

    if (!shell_path_is_directory(absolute)) {
        return SHELL_PATH_STATUS_NOT_DIRECTORY;
    }

    for (index = 0; index < paths.count; index++) {
        if (shell_text_equal(paths.absolute[index], absolute)) {
            if (stored_entry != 0 && stored_entry_size > 0) {
                shell_text_copy(
                    stored_entry,
                    stored_entry_size,
                    paths.raw[index]
                );
            }

            return SHELL_PATH_STATUS_ALREADY_PRESENT;
        }
    }

    if (paths.count >= SHELL_PATH_MAX_ENTRIES) {
        return SHELL_PATH_STATUS_TOO_MANY_ENTRIES;
    }

    if (
        shell_path_absolute_to_entry(
            absolute,
            stored,
            sizeof(stored)
        ) != 0
    ) {
        return SHELL_PATH_STATUS_INVALID;
    }

    shell_text_copy(
        paths.raw[paths.count],
        sizeof(paths.raw[paths.count]),
        stored
    );
    shell_text_copy(
        paths.absolute[paths.count],
        sizeof(paths.absolute[paths.count]),
        absolute
    );
    paths.count++;

    if (shell_path_write_list(&paths) != 0) {
        return SHELL_PATH_STATUS_IO_ERROR;
    }

    if (stored_entry != 0 && stored_entry_size > 0) {
        shell_text_copy(stored_entry, stored_entry_size, stored);
    }

    return SHELL_PATH_STATUS_OK;
}

ShellPathStatus shell_path_remove_directory(const char* input) {
    ShellPathList paths;
    char absolute[SHELL_PATH_MAX];
    int found = 0;
    int write_index = 0;
    int index;

    if (input == 0 || input[0] == '\0') {
        return SHELL_PATH_STATUS_INVALID;
    }

    if (shell_path_load_list(&paths) != 0) {
        return SHELL_PATH_STATUS_IO_ERROR;
    }

    if (
        shell_resolve_path(input, absolute, sizeof(absolute)) != 0
    ) {
        return SHELL_PATH_STATUS_INVALID;
    }

    for (index = 0; index < paths.count; index++) {
        if (shell_text_equal(paths.absolute[index], absolute)) {
            found = 1;
            continue;
        }

        if (write_index != index) {
            shell_text_copy(
                paths.raw[write_index],
                sizeof(paths.raw[write_index]),
                paths.raw[index]
            );
            shell_text_copy(
                paths.absolute[write_index],
                sizeof(paths.absolute[write_index]),
                paths.absolute[index]
            );
        }

        write_index++;
    }

    if (!found) {
        return SHELL_PATH_STATUS_NOT_FOUND;
    }

    paths.count = write_index;

    return shell_path_write_list(&paths) == 0
        ? SHELL_PATH_STATUS_OK
        : SHELL_PATH_STATUS_IO_ERROR;
}
