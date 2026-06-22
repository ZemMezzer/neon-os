#pragma once

#include "neon_fs.h"

#define PACKAGE_ROOT_PATH "0:/.packages"
#define PACKAGE_TRANSACTION_ROOT "0:/.system/.package-tmp"

#define PACKAGE_NAME_MAX 128
#define PACKAGE_DISPLAY_NAME_MAX 128
#define PACKAGE_VERSION_MAX 64
#define PACKAGE_DESCRIPTION_MAX 256
#define PACKAGE_MANIFEST_MAX 2048

#define PACKAGE_MANIFEST_NAME "package.txt"

typedef enum PackageStatus {
    PACKAGE_OK = 0,
    PACKAGE_OK_CLEANUP_PENDING,
    PACKAGE_ERR_INVALID_ARGUMENT,
    PACKAGE_ERR_NAME,
    PACKAGE_ERR_PATH_TOO_LONG,
    PACKAGE_ERR_ARCHIVE,
    PACKAGE_ERR_FILESYSTEM,
    PACKAGE_ERR_NOT_INSTALLED,
    PACKAGE_ERR_NOT_DIRECTORY,
    PACKAGE_ERR_NOT_PACKAGE,
    PACKAGE_ERR_MANIFEST,
    PACKAGE_ERR_TRANSACTION
} PackageStatus;

typedef struct PackageInfo {
    char id[PACKAGE_NAME_MAX];
    char path[NEON_FS_PATH_MAX];
    char name[PACKAGE_DISPLAY_NAME_MAX];
    char version[PACKAGE_VERSION_MAX];
    char description[PACKAGE_DESCRIPTION_MAX];
    char icon_path[NEON_FS_PATH_MAX];
    int icon_exists;
} PackageInfo;

typedef void (*PackageListVisitor)(
    const PackageInfo* info,
    void* user_data
);

int package_name_is_valid(const char* package_name);

int package_is_package_directory(const char* package_path);

PackageStatus package_name_from_archive(
    const char* archive_path,
    char* out_name,
    int out_name_size
);

PackageStatus package_prepare(void);
PackageStatus package_install_archive(
    const char* archive_path,
    const char* package_name
);

PackageStatus package_remove(const char* package_name);

PackageStatus package_get_path(
    const char* package_name,
    char* out_path,
    int out_path_size
);

PackageStatus package_get_info(
    const char* package_path,
    PackageInfo* out_info
);

PackageStatus package_get_info_by_name(
    const char* package_name,
    PackageInfo* out_info
);

PackageStatus package_list(
    PackageListVisitor visitor,
    void* user_data
);

const char* package_status_string(PackageStatus status);
