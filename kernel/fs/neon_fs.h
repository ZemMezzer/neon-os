#include "ff.h"

#define PATH_MAX 512


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
FRESULT rename_path(const char* old_path, const char* new_path);
FRESULT copy_file(const char* source_path, const char* destination_path);
FRESULT get_file_size(const char* path, FSIZE_t* out_size);