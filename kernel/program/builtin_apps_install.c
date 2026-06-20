#include <stddef.h>

#include "builtin_apps_data.h"
#include "builtin_apps_install.h"


#define BUILTIN_APPS_MARKER_PATH "0:/.neonos-defaults"
#define BUILTIN_APPS_PATH_CAPACITY 512
#define BUILTIN_APPS_WRITE_CHUNK 512


static size_t builtin_apps_strlen(const char* text) {
    size_t length = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}


static FRESULT builtin_apps_make_parent_directories(const char* file_path) {
    char path[BUILTIN_APPS_PATH_CAPACITY];
    size_t length;
    size_t index;

    if (file_path == NULL) {
        return FR_INVALID_PARAMETER;
    }

    length = builtin_apps_strlen(file_path);

    if (length == 0 || length >= sizeof(path)) {
        return FR_INVALID_NAME;
    }

    for (index = 0; index <= length; index++) {
        path[index] = file_path[index];
    }

    /*
        Bundled paths are generated as "0:/relative/path.lua". Starting
        after "0:/" prevents an attempt to create the volume root itself.
    */
    for (index = 3; index < length; index++) {
        FRESULT result;
        char saved;

        if (path[index] != '/') {
            continue;
        }

        saved = path[index];
        path[index] = '\0';

        result = f_mkdir(path);

        path[index] = saved;

        if (result != FR_OK && result != FR_EXIST) {
            return result;
        }
    }

    return FR_OK;
}


static FRESULT builtin_apps_write_file(
    const char* path,
    const unsigned char* data,
    size_t size
) {
    FIL file;
    FRESULT result;
    size_t written_total = 0;

    if (path == NULL || (data == NULL && size != 0)) {
        return FR_INVALID_PARAMETER;
    }

    result = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);

    if (result != FR_OK) {
        return result;
    }

    while (written_total < size) {
        UINT written = 0;
        size_t remaining = size - written_total;
        UINT request = remaining > BUILTIN_APPS_WRITE_CHUNK
            ? BUILTIN_APPS_WRITE_CHUNK
            : (UINT)remaining;

        result = f_write(
            &file,
            data + written_total,
            request,
            &written
        );

        if (result != FR_OK || written != request) {
            FRESULT close_result = f_close(&file);

            if (result != FR_OK) {
                return result;
            }

            if (close_result != FR_OK) {
                return close_result;
            }

            return FR_DISK_ERR;
        }

        written_total += written;
    }

    return f_close(&file);
}


FRESULT builtin_apps_install_if_needed(void) {
    FILINFO info;
    FRESULT result;
    size_t index;
    static const unsigned char marker_contents[] =
        "NeonOS default Lua apps installed\n";

    /* A completed install is never overwritten during an ordinary boot. */
    result = f_stat(BUILTIN_APPS_MARKER_PATH, &info);

    if (result == FR_OK) {
        return FR_OK;
    }

    if (result != FR_NO_FILE && result != FR_NO_PATH) {
        return result;
    }

    /*
        The marker is deliberately created last. If power is lost halfway
        through this loop, the next boot repeats the installation safely.
    */
    for (index = 0; index < neon_builtin_apps_count; index++) {
        const NeonBuiltinApp* app = &neon_builtin_apps[index];

        result = builtin_apps_make_parent_directories(app->path);

        if (result != FR_OK) {
            return result;
        }

        result = builtin_apps_write_file(app->path, app->data, app->size);

        if (result != FR_OK) {
            return result;
        }
    }

    return builtin_apps_write_file(
        BUILTIN_APPS_MARKER_PATH,
        marker_contents,
        sizeof(marker_contents) - 1
    );
}
