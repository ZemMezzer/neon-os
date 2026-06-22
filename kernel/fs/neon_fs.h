#ifndef NEON_FS_H
#define NEON_FS_H

#include <stdint.h>

#include "ff.h"

#define NEON_FS_PATH_MAX 512
#define NEON_FS_NAME_MAX 256

#ifndef PATH_MAX
#define PATH_MAX NEON_FS_PATH_MAX
#endif

typedef struct NeonFsEntry {
    char name[NEON_FS_NAME_MAX];
    FSIZE_t size;
    BYTE attributes;
} NeonFsEntry;

typedef struct NeonFsDirectory {
    DIR handle;
} NeonFsDirectory;

typedef struct NeonFsFile {
    FIL handle;
} NeonFsFile;

typedef enum NeonFsFileOpenMode {
    NEON_FS_FILE_OPEN_READ = 0,
    NEON_FS_FILE_OPEN_WRITE_TRUNCATE,
    NEON_FS_FILE_OPEN_WRITE_APPEND,
    NEON_FS_FILE_OPEN_WRITE_NEW
} NeonFsFileOpenMode;

FRESULT neon_fs_file_open(
    NeonFsFile* file,
    const char* path,
    NeonFsFileOpenMode mode
);
FRESULT neon_fs_file_read(
    NeonFsFile* file,
    void* buffer,
    UINT buffer_size,
    UINT* out_read
);
FRESULT neon_fs_file_write(
    NeonFsFile* file,
    const void* data,
    UINT data_size,
    UINT* out_written
);
FRESULT neon_fs_file_close(NeonFsFile* file);

int path_exists(const char* path);
int file_exists(const char* path);
int directory_exists(const char* path);

FRESULT create_file(const char* path);
FRESULT ensure_file(const char* path);

FRESULT write_file(const char* path, const void* data, UINT size);
FRESULT append_file(const char* path, const void* data, UINT size);

FRESULT create_directory(const char* path);
FRESULT ensure_directory(const char* path);
FRESULT ensure_directory_tree(const char* absolute_path);

FRESULT delete_path(const char* path);
FRESULT delete_tree(const char* path);

FRESULT rename_path(const char* old_path, const char* new_path);

FRESULT copy_file(const char* source_path, const char* destination_path);
FRESULT copy_path(const char* source_path, const char* destination_path);

FRESULT get_file_size(const char* path, FSIZE_t* out_size);
FRESULT get_path_info(const char* path, NeonFsEntry* out_info);

FRESULT open_directory(NeonFsDirectory* directory, const char* path);
FRESULT read_directory(
    NeonFsDirectory* directory,
    NeonFsEntry* out_entry,
    int* out_end
);
FRESULT close_directory(NeonFsDirectory* directory);

FRESULT get_free_space(const char* path, uint64_t* out_bytes);
FRESULT get_capacity(const char* path, uint64_t* out_bytes);

#endif
