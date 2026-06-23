#include "shell_alias.h"

#include "shell_io.h"
#include "shell_util.h"

#include "neon_fs.h"

#define SHELL_ALIAS_FILE "0:/.system/variables/ALIAS.txt"
#define SHELL_ALIAS_TEMP_FILE "0:/.system/variables/ALIAS.tmp"

static int shell_alias_normalize_extension_inplace(char* extension) {
    int index;
    int write_index;

    if (extension == 0) {
        return -1;
    }

    shell_text_trim_line(extension);

    if (extension[0] == '.') {
        write_index = 0;

        for (index = 1; extension[index] != '\0'; index++) {
            extension[write_index] = extension[index];
            write_index++;
        }

        extension[write_index] = '\0';
    }

    if (extension[0] == '\0') {
        return -1;
    }

    for (index = 0; extension[index] != '\0'; index++) {
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

        extension[index] = shell_text_ascii_lower(character);
    }

    return 0;
}

static int shell_alias_application_is_valid(const char* application) {
    int index;

    if (application == 0 || application[0] == '\0') {
        return 0;
    }

    for (index = 0; application[index] != '\0'; index++) {
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

int shell_alias_get_path_extension(
    const char* path,
    char* output,
    int output_size
) {
    int last_separator = -1;
    int last_dot = -1;
    int index;
    int output_index = 0;

    if (
        path == 0 ||
        path[0] == '\0' ||
        output == 0 ||
        output_size <= 1
    ) {
        return -1;
    }

    output[0] = '\0';

    for (index = 0; path[index] != '\0'; index++) {
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
        path[last_dot + 1] == '\0'
    ) {
        return 0;
    }

    for (index = last_dot + 1; path[index] != '\0'; index++) {
        if (output_index + 1 >= output_size) {
            output[0] = '\0';
            return -1;
        }

        output[output_index] = shell_text_ascii_lower(path[index]);
        output_index++;
    }

    output[output_index] = '\0';
    return 1;
}

int shell_alias_lookup_application(
    const char* extension,
    char* output,
    int output_size
) {
    NeonFsFile file;
    NeonFsEntry info;
    FRESULT result;
    char line[SHELL_LINE_MAX];

    if (
        extension == 0 ||
        extension[0] == '\0' ||
        output == 0 ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = '\0';

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
        int line_result = shell_file_read_line(&file, line, sizeof(line));

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

            shell_text_trim_line(line);

            if (line[0] == '\0' || line[0] == '#') {
                continue;
            }

            for (index = 0; line[index] != '\0'; index++) {
                if (line[index] == '=') {
                    equal_index = index;
                    break;
                }
            }

            if (equal_index < 0) {
                continue;
            }

            line[equal_index] = '\0';
            alias_extension = line;
            application = line + equal_index + 1;

            if (shell_alias_normalize_extension_inplace(alias_extension) != 0) {
                continue;
            }

            shell_text_trim_line(application);

            if (!shell_alias_application_is_valid(application)) {
                continue;
            }

            if (shell_text_equal(alias_extension, extension)) {
                shell_text_copy(output, output_size, application);

                if (neon_fs_file_close(&file) != FR_OK) {
                    output[0] = '\0';
                    return -1;
                }

                return 1;
            }
        }
    }

    return neon_fs_file_close(&file) == FR_OK ? 0 : -1;
}

static int shell_alias_parse_line_extension(
    char* line,
    char* output,
    int output_size
) {
    int equal_index = -1;
    int index;
    char* extension;
    char* application;

    if (line == 0 || output == 0 || output_size <= 1) {
        return -1;
    }

    output[0] = '\0';

    shell_text_trim_line(line);

    if (line[0] == '\0' || line[0] == '#') {
        return 0;
    }

    for (index = 0; line[index] != '\0'; index++) {
        if (line[index] == '=') {
            equal_index = index;
            break;
        }
    }

    if (equal_index < 0) {
        return 0;
    }

    line[equal_index] = '\0';
    extension = line;
    application = line + equal_index + 1;

    if (shell_alias_normalize_extension_inplace(extension) != 0) {
        return 0;
    }

    shell_text_trim_line(application);

    if (!shell_alias_application_is_valid(application)) {
        return 0;
    }

    if (shell_text_length(extension) >= output_size) {
        return -1;
    }

    shell_text_copy(output, output_size, extension);
    return 1;
}

static int shell_alias_write_raw_line(
    NeonFsFile* file,
    const char* line
) {
    int length;

    if (file == 0 || line == 0) {
        return -1;
    }

    if (shell_file_write_text(file, line) != 0) {
        return -1;
    }

    length = shell_text_length(line);

    if (length == 0 || line[length - 1] != '\n') {
        if (shell_file_write_char(file, '\n') != 0) {
            return -1;
        }
    }

    return 0;
}

static int shell_alias_write_mapping(
    NeonFsFile* file,
    const char* extension,
    const char* application
) {
    if (file == 0 || extension == 0 || application == 0) {
        return -1;
    }

    if (
        shell_file_write_text(file, extension) != 0 ||
        shell_file_write_char(file, '=') != 0 ||
        shell_file_write_text(file, application) != 0 ||
        shell_file_write_char(file, '\n') != 0
    ) {
        return -1;
    }

    return 0;
}

static int shell_alias_update_file(
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
        extension == 0 ||
        extension[0] == '\0' ||
        (!remove_only && !shell_alias_application_is_valid(application))
    ) {
        return -1;
    }

    if (out_found != 0) {
        *out_found = 0;
    }

    if (out_file_existed != 0) {
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

    if (out_file_existed != 0) {
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
        int line_result = shell_file_read_line(
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

            shell_text_copy(parsed_line, sizeof(parsed_line), raw_line);

            parse_result = shell_alias_parse_line_extension(
                parsed_line,
                parsed_extension,
                sizeof(parsed_extension)
            );

            if (
                parse_result == 1 &&
                shell_text_equal(parsed_extension, extension)
            ) {
                found = 1;

                if (!remove_only && !mapping_written) {
                    if (
                        shell_alias_write_mapping(
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

            if (shell_alias_write_raw_line(&temporary, raw_line) != 0) {
                failed = 1;
                break;
            }
        }
    }

    if (!failed && !remove_only && !mapping_written) {
        if (
            shell_alias_write_mapping(&temporary, extension, application) != 0
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

    if (out_found != 0) {
        *out_found = found;
    }

    return 0;
}

static ShellAliasStatus shell_alias_copy_and_normalize(
    const char* source,
    char* normalized,
    int normalized_size
) {
    if (source == 0 || normalized == 0 || normalized_size <= 1) {
        return SHELL_ALIAS_STATUS_INVALID_EXTENSION;
    }

    if (shell_text_length(source) >= normalized_size) {
        normalized[0] = '\0';
        return SHELL_ALIAS_STATUS_TOO_LONG;
    }

    shell_text_copy(normalized, normalized_size, source);

    return shell_alias_normalize_extension_inplace(normalized) == 0
        ? SHELL_ALIAS_STATUS_OK
        : SHELL_ALIAS_STATUS_INVALID_EXTENSION;
}

ShellAliasStatus shell_alias_add(
    const char* extension,
    const char* application,
    char* normalized_extension,
    int normalized_extension_size
) {
    char normalized[SHELL_ALIAS_EXTENSION_MAX];
    int found = 0;
    ShellAliasStatus normalize_status;

    normalize_status = shell_alias_copy_and_normalize(
        extension,
        normalized,
        sizeof(normalized)
    );

    if (normalize_status != SHELL_ALIAS_STATUS_OK) {
        return normalize_status;
    }

    if (!shell_alias_application_is_valid(application)) {
        return SHELL_ALIAS_STATUS_INVALID_APPLICATION;
    }

    if (
        shell_alias_update_file(
            normalized,
            application,
            0,
            &found,
            0
        ) != 0
    ) {
        return SHELL_ALIAS_STATUS_IO_ERROR;
    }

    if (
        normalized_extension != 0 &&
        normalized_extension_size > 0
    ) {
        shell_text_copy(
            normalized_extension,
            normalized_extension_size,
            normalized
        );
    }

    return found
        ? SHELL_ALIAS_STATUS_UPDATED
        : SHELL_ALIAS_STATUS_OK;
}

ShellAliasStatus shell_alias_remove(
    const char* extension,
    char* normalized_extension,
    int normalized_extension_size
) {
    char normalized[SHELL_ALIAS_EXTENSION_MAX];
    int found = 0;
    int file_existed = 0;
    ShellAliasStatus normalize_status;

    normalize_status = shell_alias_copy_and_normalize(
        extension,
        normalized,
        sizeof(normalized)
    );

    if (normalize_status != SHELL_ALIAS_STATUS_OK) {
        return normalize_status;
    }

    if (
        shell_alias_update_file(
            normalized,
            0,
            1,
            &found,
            &file_existed
        ) != 0
    ) {
        return SHELL_ALIAS_STATUS_IO_ERROR;
    }

    if (
        normalized_extension != 0 &&
        normalized_extension_size > 0
    ) {
        shell_text_copy(
            normalized_extension,
            normalized_extension_size,
            normalized
        );
    }

    if (!file_existed) {
        return SHELL_ALIAS_STATUS_FILE_NOT_FOUND;
    }

    return found
        ? SHELL_ALIAS_STATUS_OK
        : SHELL_ALIAS_STATUS_MAPPING_NOT_FOUND;
}

int shell_alias_build_open_command(
    const char* application,
    const char* path,
    char* output,
    int output_size
) {
    int position = 0;
    int index;

    if (
        !shell_alias_application_is_valid(application) ||
        path == 0 ||
        path[0] == '\0' ||
        output == 0 ||
        output_size <= 0
    ) {
        return -1;
    }

    for (index = 0; path[index] != '\0'; index++) {
        if (path[index] == '"') {
            return -1;
        }
    }

    output[0] = '\0';

    if (
        shell_text_append(output, output_size, &position, "open") != 0 ||
        shell_text_append_char(output, output_size, &position, ' ') != 0 ||
        shell_text_append(output, output_size, &position, application) != 0 ||
        shell_text_append_char(output, output_size, &position, ' ') != 0 ||
        shell_text_append_char(output, output_size, &position, '"') != 0 ||
        shell_text_append(output, output_size, &position, path) != 0 ||
        shell_text_append_char(output, output_size, &position, '"') != 0
    ) {
        output[0] = '\0';
        return -1;
    }

    return 0;
}
