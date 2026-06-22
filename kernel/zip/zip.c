#include "zip.h"

#include <string.h>

#define ZIP_MODE_NONE   0
#define ZIP_MODE_READER 1
#define ZIP_MODE_WRITER 2

static void zip_clear_errors(zip_t *zip) {
    zip->last_fatfs_error = FR_OK;
    zip->last_miniz_error = MZ_ZIP_NO_ERROR;
}

static zip_status zip_fail_fs(zip_t *zip, FRESULT error, zip_status status) {
    if (zip) {
        zip->last_fatfs_error = error;
    }
    return status;
}

static zip_status zip_fail_miniz(zip_t *zip) {
    mz_zip_error error;

    if (!zip) {
        return ZIP_ERR_ARCHIVE;
    }

    error = mz_zip_get_last_error(&zip->archive);
    zip->last_miniz_error = error;

    if (error == MZ_ZIP_FILE_NOT_FOUND) {
        return ZIP_ERR_NOT_FOUND;
    }

    if ((error == MZ_ZIP_UNSUPPORTED_METHOD) ||
        (error == MZ_ZIP_UNSUPPORTED_ENCRYPTION) ||
        (error == MZ_ZIP_UNSUPPORTED_FEATURE)) {
        return ZIP_ERR_UNSUPPORTED;
    }

    return ZIP_ERR_ARCHIVE;
}

static size_t zip_fatfs_read(
    void *opaque,
    mz_uint64 offset,
    void *buffer,
    size_t size
) {
    FIL *file = (FIL *)opaque;
    unsigned char *out = (unsigned char *)buffer;
    size_t total = 0;

    if (!file || (!buffer && size)) {
        return 0;
    }

    if (offset > (mz_uint64)((FSIZE_t)-1)) {
        return 0;
    }

    if (f_lseek(file, (FSIZE_t)offset) != FR_OK) {
        return 0;
    }

    while (total < size) {
        size_t remaining = size - total;
        UINT requested = (remaining > (size_t)((UINT)-1))
            ? (UINT)-1
            : (UINT)remaining;
        UINT actual = 0;

        if (f_read(file, out + total, requested, &actual) != FR_OK) {
            return total;
        }

        total += (size_t)actual;

        if (actual != requested) {
            break;
        }
    }

    return total;
}

static size_t zip_fatfs_write(
    void *opaque,
    mz_uint64 offset,
    const void *buffer,
    size_t size
) {
    FIL *file = (FIL *)opaque;
    const unsigned char *input = (const unsigned char *)buffer;
    size_t total = 0;

    if (!file || (!buffer && size)) {
        return 0;
    }

    if (offset > (mz_uint64)((FSIZE_t)-1)) {
        return 0;
    }

    if (f_lseek(file, (FSIZE_t)offset) != FR_OK) {
        return 0;
    }

    while (total < size) {
        size_t remaining = size - total;
        UINT requested = (remaining > (size_t)((UINT)-1))
            ? (UINT)-1
            : (UINT)remaining;
        UINT actual = 0;

        if (f_write(file, input + total, requested, &actual) != FR_OK) {
            return total;
        }

        total += (size_t)actual;

        if (actual != requested) {
            break;
        }
    }

    return total;
}

static int zip_is_safe_archive_name(const char *name, int allow_directory) {
    const char *segment;
    const char *p;

    if (!name || !name[0] || name[0] == '/' || name[0] == '\\') {
        return 0;
    }

    segment = name;

    for (p = name; ; ++p) {
        char ch = *p;

        if (ch == ':' || ch == '\\') {
            return 0;
        }

        if (ch == '/' || ch == '\0') {
            size_t length = (size_t)(p - segment);

            if (length == 0) {
                if (ch == '\0' && allow_directory && p != name && p[-1] == '/') {
                    return 1;
                }
                return 0;
            }

            if ((length == 1 && segment[0] == '.') ||
                (length == 2 && segment[0] == '.' && segment[1] == '.')) {
                return 0;
            }

            if (ch == '\0') {
                return 1;
            }

            segment = p + 1;
        }
    }
}

static zip_status zip_make_directories(zip_t *zip, const char *path) {
    char buffer[ZIP_PATH_MAX];
    size_t length;
    size_t first_component;
    size_t i;

    if (!path || !path[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    length = strlen(path);
    if (length >= sizeof(buffer)) {
        return ZIP_ERR_PATH_TOO_LONG;
    }

    memcpy(buffer, path, length + 1U);

    if (buffer[0] && buffer[1] == ':') {
        first_component = (buffer[2] == '/') ? 3U : 2U;
    } else if (buffer[0] == '/') {
        first_component = 1U;
    } else {
        first_component = 0U;
    }

    for (i = first_component; ; ++i) {
        if (buffer[i] == '/' || buffer[i] == '\0') {
            char saved = buffer[i];

            buffer[i] = '\0';

            if (i > first_component) {
                FRESULT result = f_mkdir(buffer);
                if (result != FR_OK && result != FR_EXIST) {
                    return zip_fail_fs(zip, result, ZIP_ERR_IO);
                }
            }

            buffer[i] = saved;

            if (saved == '\0') {
                break;
            }
        }
    }

    return ZIP_OK;
}

static zip_status zip_make_parent_directories(zip_t *zip, const char *path) {
    char buffer[ZIP_PATH_MAX];
    char *last_slash = NULL;
    size_t length;
    size_t i;

    if (!path || !path[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    length = strlen(path);
    if (length >= sizeof(buffer)) {
        return ZIP_ERR_PATH_TOO_LONG;
    }

    memcpy(buffer, path, length + 1U);

    for (i = 0; buffer[i]; ++i) {
        if (buffer[i] == '/') {
            last_slash = &buffer[i];
        }
    }

    if (!last_slash) {
        return ZIP_OK;
    }

    if (last_slash == &buffer[2] && buffer[0] && buffer[1] == ':') {
        return ZIP_OK;
    }

    *last_slash = '\0';
    return zip_make_directories(zip, buffer);
}

static zip_status zip_join_path(
    const char *root,
    const char *entry_name,
    char *out,
    size_t out_size
) {
    size_t root_length;
    size_t entry_length;
    int needs_slash;

    if (!root || !root[0] || !entry_name || !entry_name[0] || !out || !out_size) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    root_length = strlen(root);
    entry_length = strlen(entry_name);
    needs_slash = root[root_length - 1U] != '/';

    if (root_length + (size_t)needs_slash + entry_length >= out_size) {
        return ZIP_ERR_PATH_TOO_LONG;
    }

    memcpy(out, root, root_length);
    if (needs_slash) {
        out[root_length++] = '/';
    }
    memcpy(out + root_length, entry_name, entry_length + 1U);

    return ZIP_OK;
}

static size_t zip_source_read(
    void *opaque,
    mz_uint64 offset,
    void *buffer,
    size_t size
) {
    return zip_fatfs_read(opaque, offset, buffer, size);
}

static size_t zip_destination_write(
    void *opaque,
    mz_uint64 offset,
    const void *buffer,
    size_t size
) {
    return zip_fatfs_write(opaque, offset, buffer, size);
}

static zip_status zip_extract_index_to_path(
    zip_t *zip,
    mz_uint file_index,
    const char *destination_path
) {
    FIL destination;
    FRESULT result;
    mz_zip_archive_file_stat stat;
    zip_status status;

    if (!zip || !zip->is_open || zip->is_writer || !destination_path) {
        return ZIP_ERR_BAD_STATE;
    }

    if (!mz_zip_reader_file_stat(&zip->archive, file_index, &stat)) {
        return zip_fail_miniz(zip);
    }

    if (stat.m_is_directory) {
        return zip_make_directories(zip, destination_path);
    }

    if (!stat.m_is_supported) {
        return ZIP_ERR_UNSUPPORTED;
    }

    status = zip_make_parent_directories(zip, destination_path);
    if (status != ZIP_OK) {
        return status;
    }

    result = f_open(&destination, destination_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK) {
        return zip_fail_fs(zip, result, ZIP_ERR_OPEN);
    }

    if (!mz_zip_reader_extract_to_callback(
            &zip->archive,
            file_index,
            zip_destination_write,
            &destination,
            0)) {
        status = zip_fail_miniz(zip);
    } else {
        status = ZIP_OK;
    }

    result = f_sync(&destination);
    if (status == ZIP_OK && result != FR_OK) {
        status = zip_fail_fs(zip, result, ZIP_ERR_IO);
    }

    result = f_close(&destination);
    if (status == ZIP_OK && result != FR_OK) {
        status = zip_fail_fs(zip, result, ZIP_ERR_IO);
    }

    return status;
}

void zip_init(zip_t *zip) {
    if (!zip) {
        return;
    }

    memset(zip, 0, sizeof(*zip));
    zip_clear_errors(zip);
}

zip_status zip_create(zip_t *zip, const char *archive_path) {
    FRESULT result;

    if (!zip || !archive_path || !archive_path[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    if (zip->is_open) {
        return ZIP_ERR_BAD_STATE;
    }

    zip_init(zip);

    result = f_open(
        &zip->file,
        archive_path,
        FA_CREATE_ALWAYS | FA_WRITE | FA_READ
    );
    if (result != FR_OK) {
        return zip_fail_fs(zip, result, ZIP_ERR_OPEN);
    }

    mz_zip_zero_struct(&zip->archive);
    zip->archive.m_pWrite = zip_fatfs_write;
    zip->archive.m_pIO_opaque = &zip->file;

    if (!mz_zip_writer_init(&zip->archive, 0)) {
        zip_status status = zip_fail_miniz(zip);
        (void)f_close(&zip->file);
        return status;
    }

    zip->is_open = 1;
    zip->is_writer = 1;
    return ZIP_OK;
}

zip_status zip_open(zip_t *zip, const char *archive_path) {
    FRESULT result;
    FSIZE_t size;

    if (!zip || !archive_path || !archive_path[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    if (zip->is_open) {
        return ZIP_ERR_BAD_STATE;
    }

    zip_init(zip);

    result = f_open(&zip->file, archive_path, FA_READ);
    if (result != FR_OK) {
        return zip_fail_fs(zip, result, ZIP_ERR_OPEN);
    }

    size = f_size(&zip->file);
    if (!size) {
        (void)f_close(&zip->file);
        return ZIP_ERR_ARCHIVE;
    }

    mz_zip_zero_struct(&zip->archive);
    zip->archive.m_pRead = zip_fatfs_read;
    zip->archive.m_pIO_opaque = &zip->file;

    if (!mz_zip_reader_init(&zip->archive, (mz_uint64)size, 0)) {
        zip_status status = zip_fail_miniz(zip);
        (void)f_close(&zip->file);
        return status;
    }

    zip->is_open = 1;
    zip->is_writer = 0;
    return ZIP_OK;
}

zip_status zip_add_memory(
    zip_t *zip,
    const char *archive_name,
    const void *data,
    size_t size,
    zip_compression compression
) {
    if (!zip || !zip->is_open || !zip->is_writer) {
        return ZIP_ERR_BAD_STATE;
    }

    if (!zip_is_safe_archive_name(archive_name, 0) || (!data && size)) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    if (!mz_zip_writer_add_mem(
            &zip->archive,
            archive_name,
            data,
            size,
            (mz_uint)compression)) {
        return zip_fail_miniz(zip);
    }

    return ZIP_OK;
}

zip_status zip_add_file(
    zip_t *zip,
    const char *archive_name,
    const char *source_path,
    zip_compression compression
) {
    FIL source;
    FRESULT result;
    FSIZE_t source_size;
    zip_status status;

    if (!zip || !zip->is_open || !zip->is_writer) {
        return ZIP_ERR_BAD_STATE;
    }

    if (!zip_is_safe_archive_name(archive_name, 0) || !source_path || !source_path[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    result = f_open(&source, source_path, FA_READ);
    if (result != FR_OK) {
        return zip_fail_fs(zip, result, ZIP_ERR_OPEN);
    }

    source_size = f_size(&source);

    if (!mz_zip_writer_add_read_buf_callback(
            &zip->archive,
            archive_name,
            zip_source_read,
            &source,
            (mz_uint64)source_size,
            NULL,
            NULL,
            0,
            (mz_uint)compression,
            NULL,
            0,
            NULL,
            0)) {
        status = zip_fail_miniz(zip);
    } else {
        status = ZIP_OK;
    }

    result = f_close(&source);
    if (status == ZIP_OK && result != FR_OK) {
        status = zip_fail_fs(zip, result, ZIP_ERR_IO);
    }

    return status;
}

zip_status zip_add_directory(zip_t *zip, const char *archive_directory) {
    char name[ZIP_ENTRY_NAME_MAX];
    size_t length;

    if (!zip || !zip->is_open || !zip->is_writer) {
        return ZIP_ERR_BAD_STATE;
    }

    if (!archive_directory || !archive_directory[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    length = strlen(archive_directory);
    if (length + 1U >= sizeof(name)) {
        return ZIP_ERR_PATH_TOO_LONG;
    }

    memcpy(name, archive_directory, length + 1U);
    if (name[length - 1U] != '/') {
        name[length++] = '/';
        name[length] = '\0';
    }

    if (!zip_is_safe_archive_name(name, 1)) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    if (!mz_zip_writer_add_mem(&zip->archive, name, "", 0, ZIP_STORE)) {
        return zip_fail_miniz(zip);
    }

    return ZIP_OK;
}

zip_status zip_close(zip_t *zip) {
    zip_status status = ZIP_OK;
    FRESULT result;

    if (!zip) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    if (!zip->is_open) {
        return ZIP_OK;
    }

    if (zip->is_writer) {
        if (!zip->finalized) {
            if (!mz_zip_writer_finalize_archive(&zip->archive)) {
                status = zip_fail_miniz(zip);
            } else {
                zip->finalized = 1;
            }
        }

        if (!mz_zip_writer_end(&zip->archive) && status == ZIP_OK) {
            status = zip_fail_miniz(zip);
        }
    } else if (!mz_zip_reader_end(&zip->archive)) {
        status = zip_fail_miniz(zip);
    }

    if (zip->is_writer) {
        result = f_sync(&zip->file);
        if (status == ZIP_OK && result != FR_OK) {
            status = zip_fail_fs(zip, result, ZIP_ERR_IO);
        }
    }

    result = f_close(&zip->file);
    if (status == ZIP_OK && result != FR_OK) {
        status = zip_fail_fs(zip, result, ZIP_ERR_IO);
    }

    zip->is_open = 0;
    zip->is_writer = 0;
    return status;
}

void zip_abort(zip_t *zip) {
    if (!zip || !zip->is_open) {
        return;
    }

    if (zip->is_writer) {
        (void)mz_zip_writer_end(&zip->archive);
    } else {
        (void)mz_zip_reader_end(&zip->archive);
    }

    (void)f_close(&zip->file);
    zip->is_open = 0;
    zip->is_writer = 0;
}

size_t zip_count(const zip_t *zip) {
    if (!zip || !zip->is_open || zip->is_writer) {
        return 0;
    }

    return (size_t)mz_zip_reader_get_num_files((mz_zip_archive *)&zip->archive);
}

zip_status zip_get_entry(zip_t *zip, size_t index, zip_entry *entry) {
    mz_zip_archive_file_stat stat;
    mz_uint filename_size;

    if (!zip || !entry || !zip->is_open || zip->is_writer) {
        return ZIP_ERR_BAD_STATE;
    }

    if (index > (size_t)((mz_uint)-1)) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    if (!mz_zip_reader_file_stat(&zip->archive, (mz_uint)index, &stat)) {
        return zip_fail_miniz(zip);
    }

    filename_size = mz_zip_reader_get_filename(
        &zip->archive,
        (mz_uint)index,
        entry->name,
        (mz_uint)sizeof(entry->name)
    );
    if (!filename_size || filename_size >= sizeof(entry->name)) {
        return ZIP_ERR_PATH_TOO_LONG;
    }

    entry->compressed_size = stat.m_comp_size;
    entry->size = stat.m_uncomp_size;
    entry->is_directory = stat.m_is_directory ? 1 : 0;
    entry->is_supported = stat.m_is_supported ? 1 : 0;
    return ZIP_OK;
}

zip_status zip_extract_file(
    zip_t *zip,
    const char *archive_name,
    const char *destination_path
) {
    mz_uint32 index;

    if (!zip || !zip->is_open || zip->is_writer) {
        return ZIP_ERR_BAD_STATE;
    }

    if (!archive_name || !archive_name[0] || !destination_path || !destination_path[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    if (!mz_zip_reader_locate_file_v2(
            &zip->archive,
            archive_name,
            NULL,
            0,
            &index)) {
        return zip_fail_miniz(zip);
    }

    return zip_extract_index_to_path(zip, (mz_uint)index, destination_path);
}

zip_status zip_unpack(const char *archive_path, const char *destination_dir) {
    zip_t zip;
    zip_status status;
    size_t count;
    size_t i;

    if (!archive_path || !archive_path[0] || !destination_dir || !destination_dir[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    zip_init(&zip);

    status = zip_open(&zip, archive_path);
    if (status != ZIP_OK) {
        return status;
    }

    status = zip_make_directories(&zip, destination_dir);
    if (status != ZIP_OK) {
        (void)zip_close(&zip);
        return status;
    }

    count = zip_count(&zip);

    for (i = 0; i < count; ++i) {
        zip_entry entry;
        char target_path[ZIP_PATH_MAX];

        status = zip_get_entry(&zip, i, &entry);
        if (status != ZIP_OK) {
            break;
        }

        if (!zip_is_safe_archive_name(entry.name, entry.is_directory)) {
            status = ZIP_ERR_PATH;
            break;
        }

        status = zip_join_path(
            destination_dir,
            entry.name,
            target_path,
            sizeof(target_path)
        );
        if (status != ZIP_OK) {
            break;
        }

        status = zip_extract_index_to_path(&zip, (mz_uint)i, target_path);
        if (status != ZIP_OK) {
            break;
        }
    }

    {
        zip_status close_status = zip_close(&zip);
        if (status == ZIP_OK) {
            status = close_status;
        }
    }

    return status;
}

zip_status zip_validate(const char *archive_path) {
    zip_t zip;
    zip_status status;

    if (!archive_path || !archive_path[0]) {
        return ZIP_ERR_INVALID_ARGUMENT;
    }

    zip_init(&zip);

    status = zip_open(&zip, archive_path);
    if (status != ZIP_OK) {
        return status;
    }

    if (!mz_zip_validate_archive(&zip.archive, 0)) {
        status = zip_fail_miniz(&zip);
    } else {
        status = ZIP_OK;
    }

    {
        zip_status close_status = zip_close(&zip);
        if (status == ZIP_OK) {
            status = close_status;
        }
    }

    return status;
}

const char *zip_status_string(zip_status status) {
    switch (status) {
        case ZIP_OK: return "ok";
        case ZIP_ERR_INVALID_ARGUMENT: return "invalid argument";
        case ZIP_ERR_BAD_STATE: return "archive is in the wrong state";
        case ZIP_ERR_OPEN: return "cannot open file";
        case ZIP_ERR_IO: return "filesystem I/O error";
        case ZIP_ERR_ARCHIVE: return "invalid or damaged ZIP archive";
        case ZIP_ERR_NOT_FOUND: return "entry not found";
        case ZIP_ERR_UNSUPPORTED: return "unsupported ZIP feature";
        case ZIP_ERR_PATH: return "unsafe archive path";
        case ZIP_ERR_PATH_TOO_LONG: return "path is too long";
        default: return "unknown ZIP error";
    }
}

const char *zip_last_archive_error(const zip_t *zip) {
    if (!zip) {
        return "no ZIP object";
    }

    return mz_zip_get_error_string(zip->last_miniz_error);
}

FRESULT zip_last_fatfs_error(const zip_t *zip) {
    if (!zip) {
        return FR_INVALID_OBJECT;
    }

    return zip->last_fatfs_error;
}
