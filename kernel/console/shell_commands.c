#include "shell_commands.h"
#include "program_runtime.h"

#include <stddef.h>

#include "console.h"
#include "neon_fs.h"

#define SHELL_MAX_ARGS 16
#define SHELL_LINE_MAX 256
#define SHELL_PATH_MAX 512

#define SHELL_MAX_COMMANDS 32

#define SHELL_PATH_FILE "0:/system/variables/PATH.txt"
#define SHELL_PATH_MAX_ENTRIES 16

#define SHELL_ALIAS_FILE "0:/system/variables/ALIAS.txt"
#define SHELL_ALIAS_TEMP_FILE "0:/system/variables/ALIAS.tmp"
#define SHELL_ALIAS_EXTENSION_MAX 64

#define SHELL_SCRIPT_MAX_DEPTH 8

typedef struct ShellCommand {
    const char* name;
    const char* help;
    ShellCommandFn fn;
} ShellCommand;

typedef struct ShellPathList {
    char raw[SHELL_PATH_MAX_ENTRIES][SHELL_PATH_MAX];
    char absolute[SHELL_PATH_MAX_ENTRIES][SHELL_PATH_MAX];
    int count;
} ShellPathList;

static char shell_cwd[SHELL_PATH_MAX] = "0:/";
static int shell_script_depth = 0;

static ShellCommand commands[SHELL_MAX_COMMANDS];
static int command_count = 0;
static ShellCommandFallbackFn command_fallback = NULL;

static int shell_str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static int shell_str_len(const char* s) {
    int len = 0;

    while (s && s[len]) {
        len++;
    }

    return len;
}

static void shell_copy(char* dst, int dst_size, const char* src) {
    int i = 0;

    if (!dst || dst_size <= 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static int shell_append_char(char* dst, int dst_size, int* pos, char ch) {
    if (!dst || !pos || *pos < 0 || *pos + 1 >= dst_size) {
        return -1;
    }

    if (ch == '\\') {
        ch = '/';
    }

    dst[*pos] = ch;
    *pos = *pos + 1;
    dst[*pos] = 0;

    return 0;
}

static int shell_append_text(
    char* dst,
    int dst_size,
    int* pos,
    const char* text
) {
    int i = 0;

    if (!text) {
        return 0;
    }

    while (text[i]) {
        if (shell_append_char(dst, dst_size, pos, text[i]) != 0) {
            return -1;
        }

        i++;
    }

    return 0;
}

static int shell_is_volume_path(const char* path) {
    return
        path != NULL &&
        path[0] >= '0' &&
        path[0] <= '9' &&
        path[1] == ':';
}

static void shell_print_error(const char* text) {
    console_write("Error: ");
    console_write(text);
    console_write("\n");
}

static int shell_normalize_absolute_path(
    const char* input,
    char* output,
    int output_size
) {
    int read_index;
    int write_index;
    int component_restore[SHELL_PATH_MAX / 2];
    int component_count = 0;

    if (
        !input ||
        !output ||
        output_size < 4 ||
        !shell_is_volume_path(input)
    ) {
        return -1;
    }

    output[0] = input[0];
    output[1] = ':';
    output[2] = '/';
    output[3] = 0;
    write_index = 3;
    read_index = 2;

    while (input[read_index] == '/' || input[read_index] == '\\') {
        read_index++;
    }

    while (input[read_index]) {
        int component_start = read_index;
        int component_length;
        int restore_index;

        while (
            input[read_index] &&
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
                output[write_index] = 0;
            }

            continue;
        }

        if (component_count >= (int)(sizeof(component_restore) / sizeof(component_restore[0]))) {
            return -1;
        }

        restore_index = write_index;

        if (
            write_index > 3 &&
            shell_append_char(output, output_size, &write_index, '/') != 0
        ) {
            return -1;
        }

        for (int i = 0; i < component_length; i++) {
            if (
                shell_append_char(
                    output,
                    output_size,
                    &write_index,
                    input[component_start + i]
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

int shell_resolve_path(const char* input, char* out, int out_size) {
    char raw[SHELL_PATH_MAX];
    int raw_pos = 0;

    if (!input || !out || out_size <= 0) {
        return -1;
    }

    raw[0] = 0;

    if (input[0] == 0) {
        shell_copy(raw, sizeof(raw), shell_cwd);
    } else if (shell_is_volume_path(input)) {
        shell_copy(raw, sizeof(raw), input);
    } else if (input[0] == '/' || input[0] == '\\') {
        if (
            shell_append_char(raw, sizeof(raw), &raw_pos, '0') != 0 ||
            shell_append_char(raw, sizeof(raw), &raw_pos, ':') != 0 ||
            shell_append_text(raw, sizeof(raw), &raw_pos, input) != 0
        ) {
            return -1;
        }
    } else {
        if (
            shell_append_text(
                raw,
                sizeof(raw),
                &raw_pos,
                shell_cwd
            ) != 0
        ) {
            return -1;
        }

        if (
            raw_pos > 0 &&
            raw[raw_pos - 1] != '/' &&
            shell_append_char(raw, sizeof(raw), &raw_pos, '/') != 0
        ) {
            return -1;
        }

        if (
            shell_append_text(raw, sizeof(raw), &raw_pos, input) != 0
        ) {
            return -1;
        }
    }

    return shell_normalize_absolute_path(raw, out, out_size);
}

int shell_get_current_directory(
    char* output,
    int output_size
) {
    if (output == NULL || output_size <= 0) {
        return -1;
    }

    if (shell_str_len(shell_cwd) >= output_size) {
        output[0] = 0;
        return -1;
    }

    shell_copy(output, output_size, shell_cwd);
    return 0;
}

int shell_set_current_directory(const char* input) {
    char path[SHELL_PATH_MAX];
    NeonFsDirectory directory;
    FRESULT result;

    if (input == NULL || input[0] == 0) {
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

    shell_copy(shell_cwd, sizeof(shell_cwd), path);
    return 0;
}


static int shell_path_entry_to_absolute(
    const char* entry,
    char* output,
    int output_size
) {
    char raw[SHELL_PATH_MAX];
    int raw_pos = 0;
    int index = 0;

    if (!entry || !output || output_size <= 0 || entry[0] == 0) {
        return -1;
    }

    raw[0] = 0;

    if (shell_is_volume_path(entry)) {
        shell_copy(raw, sizeof(raw), entry);
    } else {
        if (
            shell_append_char(raw, sizeof(raw), &raw_pos, '0') != 0 ||
            shell_append_char(raw, sizeof(raw), &raw_pos, ':') != 0 ||
            shell_append_char(raw, sizeof(raw), &raw_pos, '/') != 0
        ) {
            return -1;
        }

        if (
            entry[0] == '.' &&
            (entry[1] == '/' || entry[1] == '\\')
        ) {
            index = 2;
        } else if (entry[0] == '/' || entry[0] == '\\') {
            index = 1;
        }

        if (
            shell_append_text(
                raw,
                sizeof(raw),
                &raw_pos,
                entry + index
            ) != 0
        ) {
            return -1;
        }
    }

    return shell_normalize_absolute_path(raw, output, output_size);
}

static int shell_absolute_to_path_entry(
    const char* absolute,
    char* output,
    int output_size
) {
    int position = 0;
    int index;

    if (
        !absolute ||
        !output ||
        output_size <= 0 ||
        !shell_is_volume_path(absolute)
    ) {
        return -1;
    }

    output[0] = 0;

    if (absolute[2] == '/') {
        if (
            shell_append_char(output, output_size, &position, '.') != 0 ||
            shell_append_char(output, output_size, &position, '/') != 0
        ) {
            return -1;
        }

        for (index = 3; absolute[index]; index++) {
            if (
                shell_append_char(
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

    shell_copy(output, output_size, absolute);
    return 0;
}

static int shell_join_path(
    const char* directory,
    const char* name,
    char* output,
    int output_size
) {
    int position = 0;

    if (!directory || !name || !output || output_size <= 0) {
        return -1;
    }

    output[0] = 0;

    if (
        shell_append_text(output, output_size, &position, directory) != 0
    ) {
        return -1;
    }

    if (
        position > 0 &&
        output[position - 1] != '/' &&
        shell_append_char(output, output_size, &position, '/') != 0
    ) {
        return -1;
    }

    return shell_append_text(output, output_size, &position, name);
}

static int shell_directory_exists(const char* path) {
    return directory_exists(path);
}


static int shell_regular_file_exists(const char* path) {
    return file_exists(path);
}


static void shell_trim_path_line(char* text) {
    int start = 0;
    int end;
    int write_index;

    if (!text) {
        return;
    }

    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }

    end = shell_str_len(text);

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

    text[end] = 0;
}

static int shell_read_line(
    NeonFsFile* file,
    char* output,
    int output_size
) {
    int length = 0;

    if (file == NULL || output == NULL || output_size <= 1) {
        return -1;
    }

    output[0] = '\0';

    for (;;) {
        char character;
        UINT read_count = 0;
        FRESULT result;

        result = neon_fs_file_read(
            file,
            &character,
            1,
            &read_count
        );

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


static int shell_write_bytes(
    NeonFsFile* file,
    const void* data,
    int size
) {
    UINT written = 0;
    FRESULT result;

    if (
        file == NULL ||
        size < 0 ||
        (size > 0 && data == NULL)
    ) {
        return -1;
    }

    result = neon_fs_file_write(
        file,
        data,
        (UINT)size,
        &written
    );

    return result == FR_OK && written == (UINT)size ? 0 : -1;
}


static int shell_write_text(NeonFsFile* file, const char* text) {
    if (text == NULL) {
        return -1;
    }

    return shell_write_bytes(file, text, shell_str_len(text));
}


static int shell_write_char(NeonFsFile* file, char character) {
    return shell_write_bytes(file, &character, 1);
}


static int shell_load_path_list(ShellPathList* list) {
    NeonFsFile file;
    char line[SHELL_PATH_MAX];
    FRESULT result;

    if (!list) {
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
        int line_result = shell_read_line(&file, line, sizeof(line));

        if (line_result == 0) {
            break;
        }

        if (line_result < 0) {
            (void)neon_fs_file_close(&file);
            return -1;
        }

        shell_trim_path_line(line);

        if (line[0] == 0 || line[0] == '#') {
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

        shell_copy(
            list->raw[list->count],
            sizeof(list->raw[list->count]),
            line
        );

        list->count++;
    }

    return neon_fs_file_close(&file) == FR_OK ? 0 : -1;
}


static char shell_ascii_to_lower(char character) {
    if (character >= 'A' && character <= 'Z') {
        return (char)(character - 'A' + 'a');
    }

    return character;
}

static int shell_get_file_extension(
    const char* path,
    char* output,
    int output_size
) {
    int last_separator = -1;
    int last_dot = -1;
    int index;
    int output_index = 0;

    if (
        path == NULL ||
        path[0] == 0 ||
        output == NULL ||
        output_size <= 1
    ) {
        return -1;
    }

    output[0] = 0;

    for (index = 0; path[index] != 0; index++) {
        if (path[index] == '/' || path[index] == '\\') {
            last_separator = index;
            last_dot = -1;
        } else if (path[index] == '.') {
            last_dot = index;
        }
    }

    if (
        last_dot < 0 ||
        last_dot <= last_separator + 1 ||
        path[last_dot + 1] == 0
    ) {
        return 0;
    }

    for (index = last_dot + 1; path[index] != 0; index++) {
        if (output_index + 1 >= output_size) {
            output[0] = 0;
            return -1;
        }

        output[output_index] = shell_ascii_to_lower(path[index]);
        output_index++;
    }

    output[output_index] = 0;
    return 1;
}

static int shell_normalize_alias_extension(char* extension) {
    int index;
    int write_index;

    if (extension == NULL) {
        return -1;
    }

    shell_trim_path_line(extension);

    if (extension[0] == '.') {
        write_index = 0;

        for (index = 1; extension[index] != 0; index++) {
            extension[write_index] = extension[index];
            write_index++;
        }

        extension[write_index] = 0;
    }

    if (extension[0] == 0) {
        return -1;
    }

    for (index = 0; extension[index] != 0; index++) {
        char character = extension[index];

        if (
            character == '/' ||
            character == '\\' ||
            character == ':' ||
            character == '=' ||
            character == '"' ||
            character == ' ' ||
            character == '\t'
        ) {
            return -1;
        }

        extension[index] = shell_ascii_to_lower(character);
    }

    return 0;
}

static int shell_alias_application_is_valid(const char* application) {
    int index;

    if (application == NULL || application[0] == 0) {
        return 0;
    }

    for (index = 0; application[index] != 0; index++) {
        if (
            application[index] == ' ' ||
            application[index] == '\t' ||
            application[index] == '\r' ||
            application[index] == '\n' ||
            application[index] == '"'
        ) {
            return 0;
        }
    }

    return 1;
}

static int shell_lookup_alias_application(
    const char* extension,
    char* output,
    int output_size
) {
    NeonFsFile file;
    NeonFsEntry info;
    FRESULT result;
    char line[SHELL_LINE_MAX];

    if (
        extension == NULL ||
        extension[0] == 0 ||
        output == NULL ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = 0;

    result = get_path_info(SHELL_ALIAS_FILE, &info);

    if (result == FR_NO_FILE || result == FR_NO_PATH) {
        return -2;
    }

    if (result != FR_OK || !file_exists(SHELL_ALIAS_FILE)) {
        return -1;
    }

    result = neon_fs_file_open(
        &file,
        SHELL_ALIAS_FILE,
        NEON_FS_FILE_OPEN_READ
    );

    if (result != FR_OK) {
        return -1;
    }

    for (;;) {
        int line_result = shell_read_line(&file, line, sizeof(line));

        if (line_result == 0) {
            break;
        }

        if (line_result < 0) {
            (void)neon_fs_file_close(&file);
            return -1;
        }

        {
            int equal_index = -1;
            int index;
            char* alias_extension;
            char* application;

            shell_trim_path_line(line);

            if (line[0] == 0 || line[0] == '#') {
                continue;
            }

            for (index = 0; line[index] != 0; index++) {
                if (line[index] == '=') {
                    equal_index = index;
                    break;
                }
            }

            if (equal_index < 0) {
                continue;
            }

            line[equal_index] = 0;
            alias_extension = line;
            application = line + equal_index + 1;

            if (shell_normalize_alias_extension(alias_extension) != 0) {
                continue;
            }

            shell_trim_path_line(application);

            if (!shell_alias_application_is_valid(application)) {
                continue;
            }

            if (shell_str_equal(alias_extension, extension)) {
                shell_copy(output, output_size, application);

                if (neon_fs_file_close(&file) != FR_OK) {
                    output[0] = 0;
                    return -1;
                }

                return 1;
            }
        }
    }

    return neon_fs_file_close(&file) == FR_OK ? 0 : -1;
}


static int shell_build_open_command(
    const char* application,
    const char* path,
    char* output,
    int output_size
) {
    int position = 0;
    int index;

    if (
        !shell_alias_application_is_valid(application) ||
        path == NULL ||
        path[0] == 0 ||
        output == NULL ||
        output_size <= 0
    ) {
        return -1;
    }

    for (index = 0; path[index] != 0; index++) {
        if (path[index] == '"') {
            return -1;
        }
    }

    output[0] = 0;

    if (
        shell_append_text(output, output_size, &position, application) != 0 ||
        shell_append_char(output, output_size, &position, ' ') != 0 ||
        shell_append_char(output, output_size, &position, '"') != 0 ||
        shell_append_text(output, output_size, &position, path) != 0 ||
        shell_append_char(output, output_size, &position, '"') != 0
    ) {
        output[0] = 0;
        return -1;
    }

    return 0;
}

static int shell_parse_alias_line_extension(
    char* line,
    char* output,
    int output_size
) {
    int equal_index = -1;
    int index;
    char* extension;
    char* application;

    if (
        line == NULL ||
        output == NULL ||
        output_size <= 1
    ) {
        return -1;
    }

    output[0] = 0;

    shell_trim_path_line(line);

    if (line[0] == 0 || line[0] == '#') {
        return 0;
    }

    for (index = 0; line[index] != 0; index++) {
        if (line[index] == '=') {
            equal_index = index;
            break;
        }
    }

    if (equal_index < 0) {
        return 0;
    }

    line[equal_index] = 0;
    extension = line;
    application = line + equal_index + 1;

    if (shell_normalize_alias_extension(extension) != 0) {
        return 0;
    }

    shell_trim_path_line(application);

    if (!shell_alias_application_is_valid(application)) {
        return 0;
    }

    if (shell_str_len(extension) >= output_size) {
        return -1;
    }

    shell_copy(output, output_size, extension);
    return 1;
}

static int shell_write_raw_alias_line(
    NeonFsFile* file,
    const char* line
) {
    int length;

    if (file == NULL || line == NULL) {
        return -1;
    }

    if (shell_write_text(file, line) != 0) {
        return -1;
    }

    length = shell_str_len(line);

    if (length == 0 || line[length - 1] != '\n') {
        if (shell_write_char(file, '\n') != 0) {
            return -1;
        }
    }

    return 0;
}


static int shell_write_alias_mapping(
    NeonFsFile* file,
    const char* extension,
    const char* application
) {
    if (
        file == NULL ||
        extension == NULL ||
        application == NULL
    ) {
        return -1;
    }

    if (
        shell_write_text(file, extension) != 0 ||
        shell_write_char(file, '=') != 0 ||
        shell_write_text(file, application) != 0 ||
        shell_write_char(file, '\n') != 0
    ) {
        return -1;
    }

    return 0;
}


static int shell_update_alias_file(
    const char* extension,
    const char* application,
    int remove_only,
    int* out_found,
    int* out_file_existed
) {
    NeonFsFile source;
    NeonFsFile temporary;
    NeonFsEntry info;
    FRESULT stat_result;
    FRESULT result;
    char raw_line[SHELL_LINE_MAX];
    char parsed_line[SHELL_LINE_MAX];
    char parsed_extension[SHELL_ALIAS_EXTENSION_MAX];
    int found = 0;
    int mapping_written = 0;
    int file_existed = 0;
    int source_opened = 0;
    int temporary_opened = 0;
    int failed = 0;

    if (
        extension == NULL ||
        extension[0] == 0 ||
        (!remove_only && !shell_alias_application_is_valid(application))
    ) {
        return -1;
    }

    if (out_found != NULL) {
        *out_found = 0;
    }

    if (out_file_existed != NULL) {
        *out_file_existed = 0;
    }

    stat_result = get_path_info(SHELL_ALIAS_FILE, &info);

    if (stat_result == FR_OK) {
        if (!file_exists(SHELL_ALIAS_FILE)) {
            return -1;
        }

        result = neon_fs_file_open(
            &source,
            SHELL_ALIAS_FILE,
            NEON_FS_FILE_OPEN_READ
        );

        if (result != FR_OK) {
            return -1;
        }

        source_opened = 1;
        file_existed = 1;
    } else if (
        stat_result != FR_NO_FILE &&
        stat_result != FR_NO_PATH
    ) {
        return -1;
    }

    if (out_file_existed != NULL) {
        *out_file_existed = file_existed;
    }

    if (remove_only && !file_existed) {
        return 0;
    }

    result = delete_path(SHELL_ALIAS_TEMP_FILE);

    if (
        result != FR_OK &&
        result != FR_NO_FILE &&
        result != FR_NO_PATH
    ) {
        if (source_opened) {
            (void)neon_fs_file_close(&source);
        }

        return -1;
    }

    result = neon_fs_file_open(
        &temporary,
        SHELL_ALIAS_TEMP_FILE,
        NEON_FS_FILE_OPEN_WRITE_TRUNCATE
    );

    if (result != FR_OK) {
        if (source_opened) {
            (void)neon_fs_file_close(&source);
        }

        return -1;
    }

    temporary_opened = 1;

    while (source_opened) {
        int line_result = shell_read_line(
            &source,
            raw_line,
            sizeof(raw_line)
        );

        if (line_result == 0) {
            break;
        }

        if (line_result < 0) {
            failed = 1;
            break;
        }

        {
            int parse_result;

            shell_copy(parsed_line, sizeof(parsed_line), raw_line);

            parse_result = shell_parse_alias_line_extension(
                parsed_line,
                parsed_extension,
                sizeof(parsed_extension)
            );

            if (
                parse_result == 1 &&
                shell_str_equal(parsed_extension, extension)
            ) {
                found = 1;

                if (!remove_only && !mapping_written) {
                    if (
                        shell_write_alias_mapping(
                            &temporary,
                            extension,
                            application
                        ) != 0
                    ) {
                        failed = 1;
                        break;
                    }

                    mapping_written = 1;
                }

                continue;
            }

            if (shell_write_raw_alias_line(&temporary, raw_line) != 0) {
                failed = 1;
                break;
            }
        }
    }

    if (!failed && !remove_only && !mapping_written) {
        if (
            shell_write_alias_mapping(
                &temporary,
                extension,
                application
            ) != 0
        ) {
            failed = 1;
        }
    }

    if (source_opened && neon_fs_file_close(&source) != FR_OK) {
        failed = 1;
    }

    if (temporary_opened && neon_fs_file_close(&temporary) != FR_OK) {
        failed = 1;
    }

    if (failed) {
        (void)delete_path(SHELL_ALIAS_TEMP_FILE);
        return -1;
    }

    if (file_existed) {
        result = delete_path(SHELL_ALIAS_FILE);

        if (result != FR_OK) {
            (void)delete_path(SHELL_ALIAS_TEMP_FILE);
            return -1;
        }
    }

    result = rename_path(SHELL_ALIAS_TEMP_FILE, SHELL_ALIAS_FILE);

    if (result != FR_OK) {
        (void)delete_path(SHELL_ALIAS_TEMP_FILE);
        return -1;
    }

    if (out_found != NULL) {
        *out_found = found;
    }

    return 0;
}


static int shell_write_path_list(const ShellPathList* list) {
    NeonFsFile file;
    FRESULT result;

    if (!list) {
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

    for (int index = 0; index < list->count; index++) {
        if (
            shell_write_text(&file, list->raw[index]) != 0 ||
            shell_write_char(&file, '\n') != 0
        ) {
            (void)neon_fs_file_close(&file);
            return -1;
        }
    }

    return neon_fs_file_close(&file) == FR_OK ? 0 : -1;
}


static int shell_name_has_path_syntax(const char* name) {
    int i = 0;

    if (!name) {
        return 0;
    }

    while (name[i]) {
        if (
            name[i] == '/' ||
            name[i] == '\\' ||
            name[i] == ':'
        ) {
            return 1;
        }

        i++;
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

    if (
        !filename ||
        filename[0] == 0 ||
        !output ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = 0;

    if (shell_name_has_path_syntax(filename)) {
        if (
            shell_resolve_path(filename, candidate, sizeof(candidate)) != 0
        ) {
            return -1;
        }

        if (shell_regular_file_exists(candidate)) {
            shell_copy(output, output_size, candidate);
            return 1;
        }

        return 0;
    }

    if (
        shell_resolve_path(filename, candidate, sizeof(candidate)) != 0
    ) {
        return -1;
    }

    if (shell_regular_file_exists(candidate)) {
        shell_copy(output, output_size, candidate);
        return 1;
    }

    if (shell_load_path_list(&paths) != 0) {
        return -1;
    }

    for (int index = 0; index < paths.count; index++) {
        if (!shell_directory_exists(paths.absolute[index])) {
            continue;
        }

        if (
            shell_join_path(
                paths.absolute[index],
                filename,
                candidate,
                sizeof(candidate)
            ) != 0
        ) {
            continue;
        }

        if (shell_regular_file_exists(candidate)) {
            shell_copy(output, output_size, candidate);
            return 1;
        }
    }

    return 0;
}

static void shell_path_parent(char* path) {
    int len;
    int i;

    if (!path || shell_str_equal(path, "0:/")) {
        return;
    }

    len = shell_str_len(path);

    for (i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            if (i <= 2) {
                path[0] = '0';
                path[1] = ':';
                path[2] = '/';
                path[3] = 0;
            } else {
                path[i] = 0;
            }

            return;
        }
    }
}

static int shell_parse_line(
    const char* line,
    char* work,
    int work_size,
    char** argv
) {
    int argc = 0;
    int wi = 0;
    int in_quote = 0;
    int token_started = 0;

    if (!line || !work || !argv) {
        return 0;
    }

    for (int i = 0; line[i] != 0; i++) {
        char ch = line[i];

        if (ch == '"') {
            if (!token_started) {
                if (argc >= SHELL_MAX_ARGS) {
                    break;
                }

                argv[argc++] = &work[wi];
                token_started = 1;
            }

            in_quote = !in_quote;
            continue;
        }

        if (!in_quote && (ch == ' ' || ch == '\t')) {
            if (token_started) {
                if (wi + 1 >= work_size) {
                    break;
                }

                work[wi++] = 0;
                token_started = 0;
            }

            continue;
        }

        if (!token_started) {
            if (argc >= SHELL_MAX_ARGS) {
                break;
            }

            argv[argc++] = &work[wi];
            token_started = 1;
        }

        if (wi + 1 >= work_size) {
            break;
        }

        work[wi++] = ch;
    }

    if (token_started && wi < work_size) {
        work[wi++] = 0;
    }

    if (wi < work_size) {
        work[wi] = 0;
    }

    return argc;
}

static int cmd_help(int argc, char** argv);
static int cmd_pwd(int argc, char** argv);
static int cmd_cd(int argc, char** argv);
static int cmd_ls(int argc, char** argv);
static int cmd_mkdir(int argc, char** argv);
static int cmd_cat(int argc, char** argv);
static int cmd_write(int argc, char** argv);
static int cmd_append(int argc, char** argv);
static int cmd_rm(int argc, char** argv);
static int cmd_mv(int argc, char** argv);
static int cmd_echo(int argc, char** argv);
static int cmd_path(int argc, char** argv);
static int cmd_alias(int argc, char** argv);
static int cmd_open(int argc, char** argv);
static int cmd_sh(int argc, char** argv);

int shell_register_command(
    const char* name,
    const char* help,
    ShellCommandFn fn
) {
    if (!name || !help || !fn) {
        return -1;
    }

    if (command_count >= SHELL_MAX_COMMANDS) {
        return -1;
    }

    commands[command_count].name = name;
    commands[command_count].help = help;
    commands[command_count].fn = fn;
    command_count++;

    return 0;
}

int shell_set_command_fallback(ShellCommandFallbackFn fn) {
    command_fallback = fn;
    return 0;
}

static int cmd_help(int argc, char** argv) {
    (void)argc;
    (void)argv;

    console_write("Commands:\n");

    for (int i = 0; i < command_count; i++) {
        console_write("  ");
        console_write(commands[i].name);
        console_write(" - ");
        console_write(commands[i].help);
        console_write("\n");
    }

    return 0;
}

static int cmd_pwd(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (shell_str_equal(shell_cwd, "0:/")) {
        console_write("/\n");
    } else {
        console_write(shell_cwd + 2);
        console_write("\n");
    }

    return 0;
}

static int cmd_cd(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    NeonFsDirectory directory;
    FRESULT result;

    if (argc < 2) {
        shell_copy(shell_cwd, sizeof(shell_cwd), "0:/");
        return 0;
    }

    if (shell_str_equal(argv[1], "..")) {
        shell_path_parent(shell_cwd);
        return 0;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    result = open_directory(&directory, path);

    if (result != FR_OK) {
        shell_print_error("directory not found");
        return -1;
    }

    if (close_directory(&directory) != FR_OK) {
        shell_print_error("cannot close directory");
        return -1;
    }

    shell_copy(shell_cwd, sizeof(shell_cwd), path);

    return 0;
}


static int cmd_ls(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    NeonFsDirectory directory;
    FRESULT result;

    if (argc >= 2) {
        if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
            shell_print_error("path too long");
            return -1;
        }
    } else {
        shell_copy(path, sizeof(path), shell_cwd);
    }

    result = open_directory(&directory, path);

    if (result != FR_OK) {
        shell_print_error("cannot open directory");
        return -1;
    }

    for (;;) {
        NeonFsEntry entry;
        int end = 0;

        result = read_directory(&directory, &entry, &end);

        if (result != FR_OK) {
            (void)close_directory(&directory);
            shell_print_error("cannot read directory");
            return -1;
        }

        if (end) {
            break;
        }

        console_write((entry.attributes & AM_DIR) ? "[D] " : "    ");
        console_write(entry.name);
        console_write("\n");
    }

    if (close_directory(&directory) != FR_OK) {
        shell_print_error("cannot close directory");
        return -1;
    }

    return 0;
}


static int cmd_mkdir(int argc, char** argv) {
    char path[SHELL_PATH_MAX];

    if (argc < 2) {
        shell_print_error("usage: mkdir <path>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    if (create_directory(path) != FR_OK) {
        shell_print_error("mkdir failed");
        return -1;
    }

    return 0;
}


static int cmd_cat(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    NeonFsFile file;
    char buffer[129];
    FRESULT result;

    if (argc < 2) {
        shell_print_error("usage: cat <file>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    result = neon_fs_file_open(
        &file,
        path,
        NEON_FS_FILE_OPEN_READ
    );

    if (result != FR_OK) {
        shell_print_error("cannot open file");
        return -1;
    }

    for (;;) {
        UINT read_count = 0;

        result = neon_fs_file_read(
            &file,
            buffer,
            (UINT)(sizeof(buffer) - 1U),
            &read_count
        );

        if (result != FR_OK) {
            (void)neon_fs_file_close(&file);
            shell_print_error("cannot read file");
            return -1;
        }

        if (read_count == 0) {
            break;
        }

        buffer[read_count] = 0;
        console_write(buffer);
    }

    if (neon_fs_file_close(&file) != FR_OK) {
        shell_print_error("cannot close file");
        return -1;
    }

    console_write("\n");

    return 0;
}


static int cmd_write(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    FRESULT result;

    if (argc < 3) {
        shell_print_error("usage: write <file> <text>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    result = write_file(
        path,
        argv[2],
        (UINT)shell_str_len(argv[2])
    );

    if (result == FR_OK) {
        result = append_file(path, "\n", 1);
    }

    if (result != FR_OK) {
        shell_print_error("cannot write file");
        return -1;
    }

    return 0;
}


static int cmd_append(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    FRESULT result;

    if (argc < 3) {
        shell_print_error("usage: append <file> <text>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    result = append_file(
        path,
        argv[2],
        (UINT)shell_str_len(argv[2])
    );

    if (result == FR_OK) {
        result = append_file(path, "\n", 1);
    }

    if (result != FR_OK) {
        shell_print_error("cannot write file");
        return -1;
    }

    return 0;
}


static int cmd_rm(int argc, char** argv) {
    char path[SHELL_PATH_MAX];

    if (argc < 2) {
        shell_print_error("usage: rm <path>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    if (delete_path(path) != FR_OK) {
        shell_print_error("remove failed");
        return -1;
    }

    return 0;
}


static int cmd_mv(int argc, char** argv) {
    char old_path[SHELL_PATH_MAX];
    char new_path[SHELL_PATH_MAX];

    if (argc < 3) {
        shell_print_error("usage: mv <old> <new>");
        return -1;
    }

    if (shell_resolve_path(argv[1], old_path, sizeof(old_path)) != 0) {
        shell_print_error("old path too long");
        return -1;
    }

    if (shell_resolve_path(argv[2], new_path, sizeof(new_path)) != 0) {
        shell_print_error("new path too long");
        return -1;
    }

    if (rename_path(old_path, new_path) != FR_OK) {
        shell_print_error("rename failed");
        return -1;
    }

    return 0;
}


static int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            console_write(" ");
        }

        console_write(argv[i]);
    }

    console_write("\n");
    return 0;
}

static int cmd_path(int argc, char** argv) {
    ShellPathList paths;

    if (shell_load_path_list(&paths) != 0) {
        shell_print_error("cannot read " SHELL_PATH_FILE);
        return -1;
    }

    if (argc == 1) {
        console_write("PATH:\n");

        for (int i = 0; i < paths.count; i++) {
            console_write("  ");
            console_write(paths.raw[i]);
            console_write("\n");
        }

        return 0;
    }

    if (shell_str_equal(argv[1], "add")) {
        char absolute[SHELL_PATH_MAX];
        char stored[SHELL_PATH_MAX];

        if (argc != 3) {
            shell_print_error("usage: path add <directory>");
            return -1;
        }

        if (
            shell_resolve_path(argv[2], absolute, sizeof(absolute)) != 0 ||
            !shell_directory_exists(absolute)
        ) {
            shell_print_error("directory not found");
            return -1;
        }

        for (int i = 0; i < paths.count; i++) {
            if (shell_str_equal(paths.absolute[i], absolute)) {
                console_write("path: already present\n");
                return 0;
            }
        }

        if (paths.count >= SHELL_PATH_MAX_ENTRIES) {
            shell_print_error("too many PATH entries");
            return -1;
        }

        if (
            shell_absolute_to_path_entry(
                absolute,
                stored,
                sizeof(stored)
            ) != 0
        ) {
            shell_print_error("path too long");
            return -1;
        }

        shell_copy(
            paths.raw[paths.count],
            sizeof(paths.raw[paths.count]),
            stored
        );
        shell_copy(
            paths.absolute[paths.count],
            sizeof(paths.absolute[paths.count]),
            absolute
        );
        paths.count++;

        if (shell_write_path_list(&paths) != 0) {
            shell_print_error("cannot write " SHELL_PATH_FILE);
            return -1;
        }

        console_write("path: added ");
        console_write(stored);
        console_write("\n");
        return 0;
    }

    if (shell_str_equal(argv[1], "remove")) {
        char absolute[SHELL_PATH_MAX];
        int found = 0;
        int write_index = 0;

        if (argc != 3) {
            shell_print_error("usage: path remove <directory>");
            return -1;
        }

        if (
            shell_resolve_path(argv[2], absolute, sizeof(absolute)) != 0
        ) {
            shell_print_error("path too long");
            return -1;
        }

        for (int i = 0; i < paths.count; i++) {
            if (shell_str_equal(paths.absolute[i], absolute)) {
                found = 1;
                continue;
            }

            if (write_index != i) {
                shell_copy(
                    paths.raw[write_index],
                    sizeof(paths.raw[write_index]),
                    paths.raw[i]
                );
                shell_copy(
                    paths.absolute[write_index],
                    sizeof(paths.absolute[write_index]),
                    paths.absolute[i]
                );
            }

            write_index++;
        }

        if (!found) {
            shell_print_error("PATH entry not found");
            return -1;
        }

        paths.count = write_index;

        if (shell_write_path_list(&paths) != 0) {
            shell_print_error("cannot write " SHELL_PATH_FILE);
            return -1;
        }

        console_write("path: removed\n");
        return 0;
    }

    shell_print_error("usage: path [add <directory> | remove <directory>]");
    return -1;
}

static int cmd_alias(int argc, char** argv) {
    char extension[SHELL_ALIAS_EXTENSION_MAX];
    int found;
    int file_existed;

    if (argc < 2) {
        shell_print_error(
            "usage: alias add <extension> <application> | alias remove <extension>"
        );
        return -1;
    }

    if (shell_str_equal(argv[1], "add")) {
        int result;

        if (argc != 4) {
            shell_print_error("usage: alias add <extension> <application>");
            return -1;
        }

        if (shell_str_len(argv[2]) >= (int)sizeof(extension)) {
            shell_print_error("extension is too long");
            return -1;
        }

        shell_copy(extension, sizeof(extension), argv[2]);

        if (shell_normalize_alias_extension(extension) != 0) {
            shell_print_error("invalid extension");
            return -1;
        }

        if (!shell_alias_application_is_valid(argv[3])) {
            shell_print_error("application must be one command name");
            return -1;
        }

        result = shell_update_alias_file(
            extension,
            argv[3],
            0,
            &found,
            &file_existed
        );

        if (result != 0) {
            shell_print_error("cannot write " SHELL_ALIAS_FILE);
            return -1;
        }

        console_write("alias: ");
        console_write(found ? "updated ." : "added .");
        console_write(extension);
        console_write(" -> ");
        console_write(argv[3]);
        console_write("\n");

        return 0;
    }

    if (shell_str_equal(argv[1], "remove")) {
        int result;

        if (argc != 3) {
            shell_print_error("usage: alias remove <extension>");
            return -1;
        }

        if (shell_str_len(argv[2]) >= (int)sizeof(extension)) {
            shell_print_error("extension is too long");
            return -1;
        }

        shell_copy(extension, sizeof(extension), argv[2]);

        if (shell_normalize_alias_extension(extension) != 0) {
            shell_print_error("invalid extension");
            return -1;
        }

        result = shell_update_alias_file(
            extension,
            NULL,
            1,
            &found,
            &file_existed
        );

        if (result != 0) {
            shell_print_error("cannot write " SHELL_ALIAS_FILE);
            return -1;
        }

        if (!file_existed) {
            shell_print_error("ALIAS.txt not found");
            return -1;
        }

        if (!found) {
            console_write("alias: no association for .");
            console_write(extension);
            console_write("\n");
            return -1;
        }

        console_write("alias: removed .");
        console_write(extension);
        console_write("\n");

        return 0;
    }

    shell_print_error(
        "usage: alias add <extension> <application> | alias remove <extension>"
    );
    return -1;
}

static int cmd_open(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    char extension[SHELL_ALIAS_EXTENSION_MAX];
    char application[SHELL_PATH_MAX];
    char command[SHELL_LINE_MAX];
    int extension_result;
    int alias_result;

    if (argc != 2) {
        shell_print_error("usage: open <file>");
        return -1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    if (!shell_regular_file_exists(path)) {
        shell_print_error("file not found");
        return -1;
    }

    extension_result = shell_get_file_extension(
        path,
        extension,
        sizeof(extension)
    );

    if (extension_result < 0) {
        shell_print_error("file extension is too long");
        return -1;
    }

    if (extension_result == 0) {
        shell_print_error("file has no extension");
        return -1;
    }

    alias_result = shell_lookup_alias_application(
        extension,
        application,
        sizeof(application)
    );

    if (alias_result == -2) {
        shell_print_error(
            "no file associations configured (ALIAS.txt not found)"
        );
        return -1;
    }

    if (alias_result < 0) {
        shell_print_error("cannot read " SHELL_ALIAS_FILE);
        return -1;
    }

    if (alias_result == 0) {
        console_write("Error: no application associated with .");
        console_write(extension);
        console_write("\n");
        return -1;
    }

    if (
        shell_build_open_command(
            application,
            path,
            command,
            sizeof(command)
        ) != 0
    ) {
        shell_print_error("cannot build application command");
        return -1;
    }

    return shell_commands_execute(command);
}

static void shell_trim_script_line(char* text) {
    int start = 0;
    int end;
    int write_index;

    if (!text) {
        return;
    }

    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }

    end = shell_str_len(text);

    while (
        end > start &&
        (
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

    text[end] = 0;
}

int shell_run_script(const char* input_path) {
    char path[SHELL_PATH_MAX];
    char line[SHELL_LINE_MAX];
    NeonFsFile file;
    FRESULT result;
    int script_status = 0;

    if (shell_script_depth >= SHELL_SCRIPT_MAX_DEPTH) {
        shell_print_error("script nesting limit reached");
        return -1;
    }

    if (shell_resolve_path(input_path, path, sizeof(path)) != 0) {
        shell_print_error("script path too long");
        return -1;
    }

    result = neon_fs_file_open(
        &file,
        path,
        NEON_FS_FILE_OPEN_READ
    );

    if (result != FR_OK) {
        shell_print_error("cannot open script");
        return -1;
    }

    shell_script_depth++;

    for (;;) {
        int line_result = shell_read_line(&file, line, sizeof(line));

        if (line_result == 0) {
            break;
        }

        if (line_result < 0) {
            (void)neon_fs_file_close(&file);
            shell_script_depth--;
            shell_print_error("cannot read script");
            return -1;
        }

        {
            int status;

            shell_trim_script_line(line);

            if (line[0] == 0 || line[0] == '#') {
                continue;
            }

            console_write("> ");
            console_write(line);
            console_write("\n");

            status = shell_commands_execute(line);

            if (status != 0) {
                script_status = status;
            }
        }
    }

    if (neon_fs_file_close(&file) != FR_OK) {
        shell_script_depth--;
        shell_print_error("cannot close script");
        return -1;
    }

    shell_script_depth--;

    return script_status;
}


static int cmd_sh(int argc, char** argv) {
    if (argc != 2) {
        shell_print_error("usage: sh <script>");
        return -1;
    }

    return shell_run_script(argv[1]);
}

int shell_commands_execute(const char* line) {
    char work[SHELL_LINE_MAX];
    char* argv[SHELL_MAX_ARGS];
    int argc;

    if (!line || line[0] == 0) {
        return 0;
    }

    argc = shell_parse_line(line, work, sizeof(work), argv);

    if (argc == 0) {
        return 0;
    }

    for (int i = 0; i < command_count; i++) {
        if (shell_str_equal(argv[0], commands[i].name)) {
            return commands[i].fn(argc, argv);
        }
    }

    if (command_fallback) {
        int fallback_status = 0;

        if (command_fallback(argc, argv, &fallback_status) != 0) {
            return fallback_status;
        }
    }

    console_write("Unknown command: ");
    console_write(argv[0]);
    console_write("\n");

    return 127;
}

void shell_commands_init(void) {
    program_set_command_executor(shell_commands_execute);

    (void)shell_register_command("help", "show commands", cmd_help);
    (void)shell_register_command("pwd", "show current directory", cmd_pwd);
    (void)shell_register_command("cd", "change directory", cmd_cd);
    (void)shell_register_command("ls", "list directory", cmd_ls);
    (void)shell_register_command("mkdir", "create directory", cmd_mkdir);
    (void)shell_register_command("cat", "print file", cmd_cat);
    (void)shell_register_command("write", "write text to file", cmd_write);
    (void)shell_register_command("append", "append text to file", cmd_append);
    (void)shell_register_command("rm", "remove file or empty dir", cmd_rm);
    (void)shell_register_command("mv", "rename or move", cmd_mv);
    (void)shell_register_command("echo", "print text", cmd_echo);
    (void)shell_register_command("path", "show or edit command PATH", cmd_path);
    (void)shell_register_command("alias", "add or remove file associations", cmd_alias);
    (void)shell_register_command("open", "open file with ALIAS.txt association", cmd_open);
    (void)shell_register_command("sh", "run a shell script", cmd_sh);
}
