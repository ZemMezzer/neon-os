#include "package_manager.h"

#include <stddef.h>

#include "shell_commands.h"
#include "zip.h"

#define PACKAGE_PATH_MAX NEON_FS_PATH_MAX

static int package_string_length(const char* text) {
    int length = 0;

    while (text != NULL && text[length] != '\0') {
        length++;
    }

    return length;
}

static int package_string_equal(const char* left, const char* right) {
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

static int package_ascii_lower(int value) {
    if (value >= 'A' && value <= 'Z') {
        return value - 'A' + 'a';
    }

    return value;
}

static int package_copy(
    char* output,
    int output_size,
    const char* text
) {
    int index = 0;

    if (
        output == NULL ||
        output_size <= 0 ||
        text == NULL
    ) {
        return -1;
    }

    while (text[index] != '\0') {
        if (index + 1 >= output_size) {
            output[0] = '\0';
            return -1;
        }

        output[index] = text[index];
        index++;
    }

    output[index] = '\0';
    return 0;
}

static int package_append(
    char* output,
    int output_size,
    int* position,
    const char* text
) {
    int index = 0;

    if (
        output == NULL ||
        output_size <= 0 ||
        position == NULL ||
        *position < 0 ||
        text == NULL
    ) {
        return -1;
    }

    while (text[index] != '\0') {
        if (*position + 1 >= output_size) {
            return -1;
        }

        output[*position] = text[index];
        *position += 1;
        index++;
    }

    output[*position] = '\0';
    return 0;
}

static int package_join(
    const char* directory,
    const char* name,
    char* output,
    int output_size
) {
    int position = 0;
    int directory_length;

    if (
        directory == NULL ||
        name == NULL ||
        output == NULL ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = '\0';

    if (package_append(output, output_size, &position, directory) != 0) {
        return -1;
    }

    directory_length = position;

    if (
        directory_length > 0 &&
        output[directory_length - 1] != '/' &&
        output[directory_length - 1] != '\\'
    ) {
        if (position + 1 >= output_size) {
            return -1;
        }

        output[position++] = '/';
        output[position] = '\0';
    }

    return package_append(output, output_size, &position, name);
}

static int package_prefixed_name(
    const char* prefix,
    const char* package_name,
    char* output,
    int output_size
) {
    int position = 0;

    if (
        prefix == NULL ||
        package_name == NULL ||
        output == NULL ||
        output_size <= 0
    ) {
        return -1;
    }

    output[0] = '\0';

    if (
        package_append(output, output_size, &position, prefix) != 0 ||
        package_append(output, output_size, &position, package_name) != 0
    ) {
        return -1;
    }

    return 0;
}

static void package_trim(char* text) {
    int start = 0;
    int end;
    int index;

    if (text == NULL) {
        return;
    }

    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }

    end = package_string_length(text);

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
        for (index = 0; index < end - start; index++) {
            text[index] = text[start + index];
        }
    }

    text[end - start] = '\0';
}

static int package_text_is_valid(const char* text) {
    int index = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[index] != '\0') {
        unsigned char character = (unsigned char)text[index];

        if (character < 32U || character == 127U) {
            return 0;
        }

        index++;
    }

    return 1;
}

static int package_relative_path_is_valid(const char* path) {
    int segment_start = 0;
    int index;

    if (
        path == NULL ||
        path[0] == '\0' ||
        path[0] == '/' ||
        path[0] == '\\'
    ) {
        return 0;
    }

    for (index = 0;; index++) {
        char character = path[index];

        if (
            character == '\\' ||
            character == ':' ||
            (character != '\0' && (unsigned char)character < 32U)
        ) {
            return 0;
        }

        if (character == '/' || character == '\0') {
            int segment_length = index - segment_start;

            if (segment_length <= 0) {
                return 0;
            }

            if (
                (segment_length == 1 && path[segment_start] == '.') ||
                (
                    segment_length == 2 &&
                    path[segment_start] == '.' &&
                    path[segment_start + 1] == '.'
                )
            ) {
                return 0;
            }

            if (character == '\0') {
                break;
            }

            segment_start = index + 1;
        }
    }

    return 1;
}

static int package_is_dot_entry(const char* name) {
    return
        name != NULL &&
        (
            (name[0] == '.' && name[1] == '\0') ||
            (name[0] == '.' && name[1] == '.' && name[2] == '\0')
        );
}

static int package_path_basename(
    const char* path,
    char* output,
    int output_size
) {
    int start = 0;
    int index;

    if (
        path == NULL ||
        output == NULL ||
        output_size <= 0
    ) {
        return -1;
    }

    for (index = 0; path[index] != '\0'; index++) {
        if (path[index] == '/' || path[index] == '\\') {
            start = index + 1;
        }
    }

    return package_copy(output, output_size, path + start);
}

static void package_info_clear(PackageInfo* info) {
    if (info == NULL) {
        return;
    }

    info->id[0] = '\0';
    info->path[0] = '\0';
    info->name[0] = '\0';
    info->version[0] = '\0';
    info->description[0] = '\0';
    info->icon_path[0] = '\0';
    info->icon_exists = 0;
}

static FRESULT package_remove_if_present(const char* path) {
    if (!path_exists(path)) {
        return FR_OK;
    }

    return delete_tree(path);
}

static int package_select_unpacked_root(
    const char* staging,
    char* output,
    int output_size
) {
    NeonFsDirectory directory;
    FRESULT result;
    char only_directory[PACKAGE_PATH_MAX];
    int directory_count = 0;
    int file_count = 0;

    if (
        staging == NULL ||
        output == NULL ||
        output_size <= 0
    ) {
        return -1;
    }

    result = open_directory(&directory, staging);

    if (result != FR_OK) {
        return -1;
    }

    for (;;) {
        NeonFsEntry entry;
        int end = 0;

        result = read_directory(&directory, &entry, &end);

        if (result != FR_OK) {
            (void)close_directory(&directory);
            return -1;
        }

        if (end) {
            break;
        }

        if (package_is_dot_entry(entry.name)) {
            continue;
        }

        if ((entry.attributes & AM_DIR) != 0) {
            directory_count++;

            if (
                directory_count == 1 &&
                package_join(
                    staging,
                    entry.name,
                    only_directory,
                    sizeof(only_directory)
                ) != 0
            ) {
                (void)close_directory(&directory);
                return -1;
            }
        } else {
            file_count++;
        }
    }

    if (close_directory(&directory) != FR_OK) {
        return -1;
    }

    if (file_count == 0 && directory_count == 1) {
        return package_copy(output, output_size, only_directory);
    }

    return package_copy(output, output_size, staging);
}

static PackageStatus package_build_paths(
    const char* package_name,
    char* destination,
    char* staging,
    char* backup
) {
    if (!package_name_is_valid(package_name)) {
        return PACKAGE_ERR_NAME;
    }

    if (
        package_join(
            PACKAGE_ROOT_PATH,
            package_name,
            destination,
            PACKAGE_PATH_MAX
        ) != 0 ||
        package_prefixed_name(
            PACKAGE_TRANSACTION_ROOT "/.installing-",
            package_name,
            staging,
            PACKAGE_PATH_MAX
        ) != 0 ||
        package_prefixed_name(
            PACKAGE_TRANSACTION_ROOT "/.backup-",
            package_name,
            backup,
            PACKAGE_PATH_MAX
        ) != 0
    ) {
        return PACKAGE_ERR_PATH_TOO_LONG;
    }

    return PACKAGE_OK;
}

static PackageStatus package_recover_transaction(
    const char* package_name
) {
    char destination[PACKAGE_PATH_MAX];
    char staging[PACKAGE_PATH_MAX];
    char backup[PACKAGE_PATH_MAX];
    PackageStatus status;
    FRESULT result;

    status = package_build_paths(
        package_name,
        destination,
        staging,
        backup
    );

    if (status != PACKAGE_OK) {
        return status;
    }

    if (path_exists(backup) && !path_exists(destination)) {
        result = rename_path(backup, destination);

        if (result != FR_OK) {
            return PACKAGE_ERR_TRANSACTION;
        }
    }

    result = package_remove_if_present(backup);

    if (result != FR_OK) {
        return PACKAGE_ERR_TRANSACTION;
    }

    result = package_remove_if_present(staging);

    if (result != FR_OK) {
        return PACKAGE_ERR_TRANSACTION;
    }

    return PACKAGE_OK;
}

static PackageStatus package_zip_status(zip_status status) {
    return status == ZIP_OK ? PACKAGE_OK : PACKAGE_ERR_ARCHIVE;
}

static PackageStatus package_read_manifest(
    const char* manifest_path,
    char* output,
    int output_size
) {
    NeonFsFile file;
    FRESULT result;
    int length = 0;

    if (
        manifest_path == NULL ||
        output == NULL ||
        output_size <= 1
    ) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    result = neon_fs_file_open(
        &file,
        manifest_path,
        NEON_FS_FILE_OPEN_READ
    );

    if (result != FR_OK) {
        return PACKAGE_ERR_MANIFEST;
    }

    for (;;) {
        UINT read_count = 0;
        UINT request;

        if (length + 1 >= output_size) {
            char extra;
            UINT extra_read = 0;

            output[length] = '\0';
            result = neon_fs_file_read(&file, &extra, 1, &extra_read);
            (void)neon_fs_file_close(&file);

            if (result != FR_OK) {
                return PACKAGE_ERR_FILESYSTEM;
            }

            return extra_read == 0
                ? PACKAGE_OK
                : PACKAGE_ERR_MANIFEST;
        }

        request = (UINT)(output_size - 1 - length);

        result = neon_fs_file_read(
            &file,
            output + length,
            request,
            &read_count
        );

        if (result != FR_OK) {
            (void)neon_fs_file_close(&file);
            return PACKAGE_ERR_FILESYSTEM;
        }

        if (read_count == 0) {
            output[length] = '\0';

            if (neon_fs_file_close(&file) != FR_OK) {
                return PACKAGE_ERR_FILESYSTEM;
            }

            return PACKAGE_OK;
        }

        length += (int)read_count;
    }
}

static PackageStatus package_parse_manifest(
    char* contents,
    PackageInfo* out_info
) {
    char* line;

    if (contents == NULL || out_info == NULL) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    line = contents;

    while (*line != '\0') {
        char* next = line;
        char* equals = NULL;
        char* key;
        char* value;
        int index;

        while (*next != '\0' && *next != '\n') {
            next++;
        }

        if (*next == '\n') {
            *next = '\0';
            next++;
        }

        package_trim(line);

        if (line[0] != '\0' && line[0] != '#') {
            for (index = 0; line[index] != '\0'; index++) {
                if (line[index] == '=') {
                    equals = line + index;
                    break;
                }
            }

            if (equals == NULL) {
                return PACKAGE_ERR_MANIFEST;
            }

            *equals = '\0';
            key = line;
            value = equals + 1;

            package_trim(key);
            package_trim(value);

            if (!package_text_is_valid(value)) {
                return PACKAGE_ERR_MANIFEST;
            }

            if (package_string_equal(key, "name")) {
                if (
                    package_copy(
                        out_info->name,
                        sizeof(out_info->name),
                        value
                    ) != 0
                ) {
                    return PACKAGE_ERR_MANIFEST;
                }
            } else if (package_string_equal(key, "version")) {
                if (
                    package_copy(
                        out_info->version,
                        sizeof(out_info->version),
                        value
                    ) != 0
                ) {
                    return PACKAGE_ERR_MANIFEST;
                }
            } else if (package_string_equal(key, "description")) {
                if (
                    package_copy(
                        out_info->description,
                        sizeof(out_info->description),
                        value
                    ) != 0
                ) {
                    return PACKAGE_ERR_MANIFEST;
                }
            } else if (package_string_equal(key, "icon")) {
                if (
                    value[0] != '\0' &&
                    !package_relative_path_is_valid(value)
                ) {
                    return PACKAGE_ERR_MANIFEST;
                }

                if (
                    value[0] == '\0'
                ) {
                    out_info->icon_path[0] = '\0';
                    out_info->icon_exists = 0;
                } else if (
                    package_join(
                        out_info->path,
                        value,
                        out_info->icon_path,
                        sizeof(out_info->icon_path)
                    ) != 0
                ) {
                    return PACKAGE_ERR_PATH_TOO_LONG;
                } else {
                    out_info->icon_exists = file_exists(out_info->icon_path);
                }
            }
        }

        line = next;
    }

    if (out_info->name[0] == '\0') {
        if (
            package_copy(
                out_info->name,
                sizeof(out_info->name),
                out_info->id
            ) != 0
        ) {
            return PACKAGE_ERR_MANIFEST;
        }
    }

    return PACKAGE_OK;
}

int package_name_is_valid(const char* package_name) {
    int index = 0;

    if (
        package_name == NULL ||
        package_name[0] == '\0'
    ) {
        return 0;
    }

    while (package_name[index] != '\0') {
        unsigned char character = (unsigned char)package_name[index];

        if (
            character < 32U ||
            character == '/' ||
            character == '\\' ||
            character == ':'
        ) {
            return 0;
        }

        index++;

        if (index >= PACKAGE_NAME_MAX) {
            return 0;
        }
    }

    return
        !package_string_equal(package_name, ".") &&
        !package_string_equal(package_name, "..");
}

int package_is_package_directory(const char* package_path) {
    char normalized_path[PACKAGE_PATH_MAX];
    char manifest_path[PACKAGE_PATH_MAX];

    if (
        package_path == NULL ||
        package_path[0] == '\0' ||
        shell_resolve_path(
            package_path,
            normalized_path,
            sizeof(normalized_path)
        ) != 0
    ) {
        return 0;
    }

    if (!directory_exists(normalized_path)) {
        return 0;
    }

    if (
        package_join(
            normalized_path,
            PACKAGE_MANIFEST_NAME,
            manifest_path,
            sizeof(manifest_path)
        ) != 0
    ) {
        return 0;
    }

    return file_exists(manifest_path);
}

PackageStatus package_name_from_archive(
    const char* archive_path,
    char* out_name,
    int out_name_size
) {
    int last_separator = -1;
    int end;
    int index;
    int base_start;
    int name_length;

    if (
        archive_path == NULL ||
        archive_path[0] == '\0' ||
        out_name == NULL ||
        out_name_size <= 1
    ) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    out_name[0] = '\0';

    end = package_string_length(archive_path);

    for (index = 0; index < end; index++) {
        if (
            archive_path[index] == '/' ||
            archive_path[index] == '\\'
        ) {
            last_separator = index;
        }
    }

    base_start = last_separator + 1;

    if (end - base_start <= 5) {
        return PACKAGE_ERR_NAME;
    }

    if (
        archive_path[end - 5] != '.' ||
        package_ascii_lower(archive_path[end - 4]) != 'n' ||
        package_ascii_lower(archive_path[end - 3]) != 'p' ||
        package_ascii_lower(archive_path[end - 2]) != 'k' ||
        package_ascii_lower(archive_path[end - 1]) != 'g'
    ) {
        return PACKAGE_ERR_NAME;
    }

    name_length = end - base_start - 5;

    if (name_length <= 0 || name_length + 1 > out_name_size) {
        return PACKAGE_ERR_NAME;
    }

    for (index = 0; index < name_length; index++) {
        out_name[index] = archive_path[base_start + index];
    }

    out_name[name_length] = '\0';

    return package_name_is_valid(out_name)
        ? PACKAGE_OK
        : PACKAGE_ERR_NAME;
}

PackageStatus package_prepare(void) {
    FRESULT result;

    result = ensure_directory(PACKAGE_ROOT_PATH);

    if (result != FR_OK) {
        return PACKAGE_ERR_FILESYSTEM;
    }

    result = ensure_directory_tree(PACKAGE_TRANSACTION_ROOT);

    if (result != FR_OK) {
        return PACKAGE_ERR_FILESYSTEM;
    }

    return PACKAGE_OK;
}

PackageStatus package_install_archive(
    const char* archive_path,
    const char* package_name
) {
    char destination[PACKAGE_PATH_MAX];
    char staging[PACKAGE_PATH_MAX];
    char backup[PACKAGE_PATH_MAX];
    char unpacked_root[PACKAGE_PATH_MAX];
    PackageStatus status;
    zip_status zip_result;
    FRESULT result;
    int replaced_existing = 0;
    int cleanup_pending = 0;

    if (
        archive_path == NULL ||
        archive_path[0] == '\0' ||
        !package_name_is_valid(package_name)
    ) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    if (!file_exists(archive_path)) {
        return PACKAGE_ERR_ARCHIVE;
    }

    status = package_prepare();

    if (status != PACKAGE_OK) {
        return status;
    }

    status = package_recover_transaction(package_name);

    if (status != PACKAGE_OK) {
        return status;
    }

    status = package_build_paths(
        package_name,
        destination,
        staging,
        backup
    );

    if (status != PACKAGE_OK) {
        return status;
    }

    if (path_exists(destination) && !directory_exists(destination)) {
        return PACKAGE_ERR_NOT_DIRECTORY;
    }

    zip_result = zip_unpack(archive_path, staging);

    if (zip_result != ZIP_OK) {
        (void)package_remove_if_present(staging);
        return package_zip_status(zip_result);
    }

    if (
        package_select_unpacked_root(
            staging,
            unpacked_root,
            sizeof(unpacked_root)
        ) != 0
    ) {
        (void)package_remove_if_present(staging);
        return PACKAGE_ERR_FILESYSTEM;
    }

    if (directory_exists(destination)) {
        result = rename_path(destination, backup);

        if (result != FR_OK) {
            (void)package_remove_if_present(staging);
            return PACKAGE_ERR_FILESYSTEM;
        }

        replaced_existing = 1;
    }

    result = rename_path(unpacked_root, destination);

    if (result != FR_OK) {
        if (replaced_existing) {
            (void)rename_path(backup, destination);
        }

        (void)package_remove_if_present(staging);
        return PACKAGE_ERR_FILESYSTEM;
    }

    if (!package_string_equal(unpacked_root, staging)) {
        result = package_remove_if_present(staging);

        if (result != FR_OK) {
            cleanup_pending = 1;
        }
    }

    if (replaced_existing) {
        result = package_remove_if_present(backup);

        if (result != FR_OK) {
            cleanup_pending = 1;
        }
    }

    return cleanup_pending
        ? PACKAGE_OK_CLEANUP_PENDING
        : PACKAGE_OK;
}

PackageStatus package_remove(const char* package_name) {
    char destination[PACKAGE_PATH_MAX];
    char staging[PACKAGE_PATH_MAX];
    char backup[PACKAGE_PATH_MAX];
    PackageStatus status;
    FRESULT result;

    if (!package_name_is_valid(package_name)) {
        return PACKAGE_ERR_NAME;
    }

    status = package_prepare();

    if (status != PACKAGE_OK) {
        return status;
    }

    status = package_recover_transaction(package_name);

    if (status != PACKAGE_OK) {
        return status;
    }

    status = package_build_paths(
        package_name,
        destination,
        staging,
        backup
    );

    if (status != PACKAGE_OK) {
        return status;
    }

    (void)staging;
    (void)backup;

    if (!path_exists(destination)) {
        return PACKAGE_ERR_NOT_INSTALLED;
    }

    if (!directory_exists(destination)) {
        return PACKAGE_ERR_NOT_DIRECTORY;
    }

    result = delete_tree(destination);

    if (result != FR_OK) {
        return PACKAGE_ERR_FILESYSTEM;
    }

    return PACKAGE_OK;
}

PackageStatus package_get_info(
    const char* package_path,
    PackageInfo* out_info
) {
    char normalized_path[PACKAGE_PATH_MAX];
    char manifest_path[PACKAGE_PATH_MAX];
    char contents[PACKAGE_MANIFEST_MAX];
    PackageStatus status;

    if (package_path == NULL || out_info == NULL) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    package_info_clear(out_info);

    if (
        shell_resolve_path(
            package_path,
            normalized_path,
            sizeof(normalized_path)
        ) != 0
    ) {
        return PACKAGE_ERR_PATH_TOO_LONG;
    }

    if (!package_is_package_directory(normalized_path)) {
        return PACKAGE_ERR_NOT_PACKAGE;
    }

    if (
        package_copy(
            out_info->path,
            sizeof(out_info->path),
            normalized_path
        ) != 0 ||
        package_path_basename(
            normalized_path,
            out_info->id,
            sizeof(out_info->id)
        ) != 0 ||
        !package_name_is_valid(out_info->id)
    ) {
        package_info_clear(out_info);
        return PACKAGE_ERR_NAME;
    }

    if (
        package_copy(
            out_info->name,
            sizeof(out_info->name),
            out_info->id
        ) != 0 ||
        package_join(
            normalized_path,
            PACKAGE_MANIFEST_NAME,
            manifest_path,
            sizeof(manifest_path)
        ) != 0
    ) {
        package_info_clear(out_info);
        return PACKAGE_ERR_PATH_TOO_LONG;
    }

    status = package_read_manifest(
        manifest_path,
        contents,
        sizeof(contents)
    );

    if (status != PACKAGE_OK) {
        package_info_clear(out_info);
        return status;
    }

    status = package_parse_manifest(contents, out_info);

    if (status != PACKAGE_OK) {
        package_info_clear(out_info);
    }

    return status;
}

PackageStatus package_get_path(
    const char* package_name,
    char* out_path,
    int out_path_size
) {
    char package_path[PACKAGE_PATH_MAX];
    int found;

    if (
        package_name == NULL ||
        out_path == NULL ||
        out_path_size <= 1
    ) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    out_path[0] = '\0';

    if (!package_name_is_valid(package_name)) {
        return PACKAGE_ERR_NAME;
    }

    found = shell_find_path(
        package_name,
        package_path,
        sizeof(package_path)
    );

    if (found < 0) {
        return PACKAGE_ERR_FILESYSTEM;
    }

    if (found == 0) {
        return PACKAGE_ERR_NOT_INSTALLED;
    }

    if (!package_is_package_directory(package_path)) {
        return PACKAGE_ERR_NOT_PACKAGE;
    }

    if (package_copy(out_path, out_path_size, package_path) != 0) {
        return PACKAGE_ERR_PATH_TOO_LONG;
    }

    return PACKAGE_OK;
}

PackageStatus package_get_info_by_name(
    const char* package_name,
    PackageInfo* out_info
) {
    char package_path[PACKAGE_PATH_MAX];
    PackageStatus status;

    if (out_info == NULL) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    status = package_get_path(
        package_name,
        package_path,
        sizeof(package_path)
    );

    if (status != PACKAGE_OK) {
        return status;
    }

    return package_get_info(package_path, out_info);
}

typedef struct PackageListContext {
    PackageListVisitor visitor;
    void* user_data;
    PackageStatus status;
} PackageListContext;

static int package_list_path(
    const char* path_directory,
    int path_index,
    void* user_data
) {
    NeonFsDirectory directory;
    PackageListContext* context = (PackageListContext*)user_data;
    FRESULT result;

    (void)path_index;

    if (path_directory == NULL || context == NULL) {
        return -1;
    }

    result = open_directory(&directory, path_directory);

    if (result != FR_OK) {
        return -1;
    }

    for (;;) {
        NeonFsEntry entry;
        char package_path[PACKAGE_PATH_MAX];
        PackageInfo info;
        PackageStatus status;
        int end = 0;

        result = read_directory(&directory, &entry, &end);

        if (result != FR_OK) {
            (void)close_directory(&directory);
            return -1;
        }

        if (end) {
            break;
        }

        if (
            (entry.attributes & AM_DIR) == 0 ||
            package_is_dot_entry(entry.name)
        ) {
            continue;
        }

        if (
            package_join(
                path_directory,
                entry.name,
                package_path,
                sizeof(package_path)
            ) != 0
        ) {
            context->status = PACKAGE_ERR_PATH_TOO_LONG;
            (void)close_directory(&directory);
            return -1;
        }

        if (!package_is_package_directory(package_path)) {
            continue;
        }

        status = package_get_info(package_path, &info);

        if (status != PACKAGE_OK) {
            context->status = status;
            (void)close_directory(&directory);
            return -1;
        }

        context->visitor(&info, context->user_data);
    }

    return close_directory(&directory) == FR_OK ? 0 : -1;
}

PackageStatus package_list(
    PackageListVisitor visitor,
    void* user_data
) {
    PackageListContext context;
    int result;

    if (visitor == NULL) {
        return PACKAGE_ERR_INVALID_ARGUMENT;
    }

    context.visitor = visitor;
    context.user_data = user_data;
    context.status = PACKAGE_OK;

    result = shell_visit_path_directories(
        package_list_path,
        &context
    );

    if (context.status != PACKAGE_OK) {
        return context.status;
    }

    return result == 0 ? PACKAGE_OK : PACKAGE_ERR_FILESYSTEM;
}

const char* package_status_string(PackageStatus status) {
    switch (status) {
        case PACKAGE_OK:
            return "ok";
        case PACKAGE_OK_CLEANUP_PENDING:
            return "installed, cleanup pending";
        case PACKAGE_ERR_INVALID_ARGUMENT:
            return "invalid argument";
        case PACKAGE_ERR_NAME:
            return "invalid package name";
        case PACKAGE_ERR_PATH_TOO_LONG:
            return "path is too long";
        case PACKAGE_ERR_ARCHIVE:
            return "invalid or unsupported package archive";
        case PACKAGE_ERR_FILESYSTEM:
            return "filesystem error";
        case PACKAGE_ERR_NOT_INSTALLED:
            return "package not found";
        case PACKAGE_ERR_NOT_DIRECTORY:
            return "package path is not a folder";
        case PACKAGE_ERR_NOT_PACKAGE:
            return "directory does not contain package.txt";
        case PACKAGE_ERR_MANIFEST:
            return "invalid package.txt";
        case PACKAGE_ERR_TRANSACTION:
            return "cannot recover package transaction";
        default:
            return "unknown package error";
    }
}
