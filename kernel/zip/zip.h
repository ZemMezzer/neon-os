#include <stddef.h>

#include "ff.h"
#include "miniz.h"

#ifndef ZIP_PATH_MAX
#define ZIP_PATH_MAX 512U
#endif

#ifndef ZIP_ENTRY_NAME_MAX
#define ZIP_ENTRY_NAME_MAX 256U
#endif

typedef enum zip_status {
    ZIP_OK = 0,
    ZIP_ERR_INVALID_ARGUMENT,
    ZIP_ERR_BAD_STATE,
    ZIP_ERR_OPEN,
    ZIP_ERR_IO,
    ZIP_ERR_ARCHIVE,
    ZIP_ERR_NOT_FOUND,
    ZIP_ERR_UNSUPPORTED,
    ZIP_ERR_PATH,
    ZIP_ERR_PATH_TOO_LONG
} zip_status;

typedef enum zip_compression {
    ZIP_STORE = 0,
    ZIP_DEFLATE = 6
} zip_compression;

typedef struct zip_entry {
    char name[ZIP_ENTRY_NAME_MAX];
    mz_uint64 compressed_size;
    mz_uint64 size;
    int is_directory;
    int is_supported;
} zip_entry;

typedef struct zip {
    mz_zip_archive archive;
    FIL file;

    int is_open;
    int is_writer;
    int finalized;

    FRESULT last_fatfs_error;
    mz_zip_error last_miniz_error;
} zip_t;

void zip_init(zip_t *zip);

zip_status zip_create(zip_t *zip, const char *archive_path);

zip_status zip_open(zip_t *zip, const char *archive_path);

zip_status zip_add_memory(
    zip_t *zip,
    const char *archive_name,
    const void *data,
    size_t size,
    zip_compression compression
);

zip_status zip_add_file(
    zip_t *zip,
    const char *archive_name,
    const char *source_path,
    zip_compression compression
);

zip_status zip_add_directory(zip_t *zip, const char *archive_directory);

zip_status zip_close(zip_t *zip);

void zip_abort(zip_t *zip);

size_t zip_count(const zip_t *zip);

zip_status zip_get_entry(zip_t *zip, size_t index, zip_entry *entry);

zip_status zip_extract_file(
    zip_t *zip,
    const char *archive_name,
    const char *destination_path
);

zip_status zip_unpack(const char *archive_path, const char *destination_dir);

zip_status zip_validate(const char *archive_path);

const char *zip_status_string(zip_status status);

const char *zip_last_archive_error(const zip_t *zip);

FRESULT zip_last_fatfs_error(const zip_t *zip);
