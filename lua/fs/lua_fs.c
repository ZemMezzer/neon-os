#include <stddef.h>
#include <stdint.h>

#include "ff.h"
#include "lua_fs.h"

#include "lauxlib.h"


#define LUA_FS_PATH_CAPACITY 512
#define LUA_FS_COPY_BUFFER_SIZE 512
#define LUA_FS_MAX_RECURSION 24


static size_t lua_fs_strlen(const char* text) {
    size_t length = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}


static int lua_fs_is_separator(char c) {
    return c == '/' || c == '\\';
}

static int lua_fs_is_dot_entry(const char* name) {
    if (name == NULL) {
        return 0;
    }

    return
        (name[0] == '.' && name[1] == '\0') ||
        (name[0] == '.' && name[1] == '.' && name[2] == '\0');
}


static int lua_fs_has_embedded_nul(const char* text, size_t length) {
    size_t index;

    for (index = 0; index < length; index++) {
        if (text[index] == '\0') {
            return 1;
        }
    }

    return 0;
}


static const char* lua_fs_check_path(
    lua_State* state,
    int argument_index
) {
    const char* path;
    size_t length;

    path = luaL_checklstring(state, argument_index, &length);

    if (lua_fs_has_embedded_nul(path, length)) {
        luaL_argerror(state, argument_index, "path must not contain a NUL byte");
    }

    if (length >= LUA_FS_PATH_CAPACITY) {
        luaL_argerror(state, argument_index, "path is too long");
    }

    return path;
}


static const char* lua_fs_result_name(FRESULT result) {
    switch (result) {
        case FR_OK:                  return "FR_OK";
        case FR_DISK_ERR:            return "FR_DISK_ERR";
        case FR_INT_ERR:             return "FR_INT_ERR";
        case FR_NOT_READY:           return "FR_NOT_READY";
        case FR_NO_FILE:             return "FR_NO_FILE";
        case FR_NO_PATH:             return "FR_NO_PATH";
        case FR_INVALID_NAME:        return "FR_INVALID_NAME";
        case FR_DENIED:              return "FR_DENIED";
        case FR_EXIST:               return "FR_EXIST";
        case FR_INVALID_OBJECT:      return "FR_INVALID_OBJECT";
        case FR_WRITE_PROTECTED:     return "FR_WRITE_PROTECTED";
        case FR_INVALID_DRIVE:       return "FR_INVALID_DRIVE";
        case FR_NOT_ENABLED:         return "FR_NOT_ENABLED";
        case FR_NO_FILESYSTEM:       return "FR_NO_FILESYSTEM";
        case FR_MKFS_ABORTED:        return "FR_MKFS_ABORTED";
        case FR_TIMEOUT:             return "FR_TIMEOUT";
        case FR_LOCKED:              return "FR_LOCKED";
        case FR_NOT_ENOUGH_CORE:     return "FR_NOT_ENOUGH_CORE";
        case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
        case FR_INVALID_PARAMETER:   return "FR_INVALID_PARAMETER";
        default:                     return "FR_UNKNOWN";
    }
}


static int lua_fs_raise(lua_State* state, const char* operation, FRESULT result) {
    return luaL_error(
        state,
        "fs.%s failed: %s",
        operation,
        lua_fs_result_name(result)
    );
}


static int lua_fs_is_root_path(const char* path) {
    size_t length;

    if (path == NULL || path[0] == '\0') {
        return 1;
    }

    if (lua_fs_is_separator(path[0]) && path[1] == '\0') {
        return 1;
    }

    length = lua_fs_strlen(path);

    if (length == 2 && path[1] == ':') {
        return 1;
    }

    if (
        length == 3 &&
        path[1] == ':' &&
        lua_fs_is_separator(path[2])
    ) {
        return 1;
    }

    return 0;
}


static int lua_fs_paths_equal(const char* left, const char* right) {
    size_t left_length;
    size_t right_length;

    if (left == NULL || right == NULL) {
        return 0;
    }

    left_length = lua_fs_strlen(left);
    right_length = lua_fs_strlen(right);

    while (left_length > 1 && lua_fs_is_separator(left[left_length - 1])) {
        left_length--;
    }

    while (right_length > 1 && lua_fs_is_separator(right[right_length - 1])) {
        right_length--;
    }

    if (left_length != right_length) {
        return 0;
    }

    for (size_t index = 0; index < left_length; index++) {
        char a = left[index];
        char b = right[index];

        if (lua_fs_is_separator(a)) {
            a = '/';
        }

        if (lua_fs_is_separator(b)) {
            b = '/';
        }

        if (a != b) {
            return 0;
        }
    }

    return 1;
}

static int lua_fs_path_is_child_of(const char* parent, const char* child) {
    size_t parent_length;
    size_t child_length;

    if (parent == NULL || child == NULL) {
        return 0;
    }

    parent_length = lua_fs_strlen(parent);
    child_length = lua_fs_strlen(child);

    if (
        parent_length == 1 &&
        lua_fs_is_separator(parent[0]) &&
        child_length > 1 &&
        lua_fs_is_separator(child[0])
    ) {
        return 1;
    }

    while (parent_length > 1 && lua_fs_is_separator(parent[parent_length - 1])) {
        parent_length--;
    }

    while (child_length > 1 && lua_fs_is_separator(child[child_length - 1])) {
        child_length--;
    }

    if (child_length <= parent_length) {
        return 0;
    }

    for (size_t index = 0; index < parent_length; index++) {
        char a = parent[index];
        char b = child[index];

        if (lua_fs_is_separator(a)) {
            a = '/';
        }

        if (lua_fs_is_separator(b)) {
            b = '/';
        }

        if (a != b) {
            return 0;
        }
    }

    return lua_fs_is_separator(child[parent_length]);
}


static int lua_fs_join_path(
    char* output,
    size_t output_capacity,
    const char* parent,
    const char* name
) {
    size_t parent_length;
    size_t name_length;
    size_t position;
    int parent_needs_separator;

    if (output == NULL || output_capacity == 0 || parent == NULL || name == NULL) {
        return 0;
    }

    parent_length = lua_fs_strlen(parent);
    name_length = lua_fs_strlen(name);

    while (name_length > 0 && lua_fs_is_separator(name[0])) {
        name++;
        name_length--;
    }

    parent_needs_separator =
        parent_length > 0 &&
        !lua_fs_is_separator(parent[parent_length - 1]) &&
        parent[parent_length - 1] != ':';

    position = 0;

    if (parent_length + (parent_needs_separator ? 1U : 0U) + name_length + 1U > output_capacity) {
        return 0;
    }

    for (size_t index = 0; index < parent_length; index++) {
        output[position++] = parent[index] == '\\' ? '/' : parent[index];
    }

    if (parent_needs_separator) {
        output[position++] = '/';
    }

    for (size_t index = 0; index < name_length; index++) {
        output[position++] = name[index] == '\\' ? '/' : name[index];
    }

    output[position] = '\0';
    return 1;
}


static void lua_fs_push_attributes(lua_State* state, const FILINFO* info) {
    int is_directory;

    is_directory = info != NULL && (info->fattrib & AM_DIR) != 0;

    lua_newtable(state);

    lua_pushinteger(state, (lua_Integer)(info == NULL ? 0 : info->fsize));
    lua_setfield(state, -2, "size");

    lua_pushboolean(state, is_directory);
    lua_setfield(state, -2, "is_dir");

    lua_pushboolean(state, is_directory);
    lua_setfield(state, -2, "isDir");

    lua_pushboolean(state, info != NULL && (info->fattrib & AM_RDO) != 0);
    lua_setfield(state, -2, "readonly");

    lua_pushboolean(state, info != NULL && (info->fattrib & AM_HID) != 0);
    lua_setfield(state, -2, "hidden");

    lua_pushboolean(state, info != NULL && (info->fattrib & AM_SYS) != 0);
    lua_setfield(state, -2, "system");

    lua_pushboolean(state, info != NULL && (info->fattrib & AM_ARC) != 0);
    lua_setfield(state, -2, "archive");
}


static int lua_fs_list(lua_State* state) {
    const char* path;
    DIR directory;
    FILINFO info;
    FRESULT result;
    int index;

    path = lua_fs_check_path(state, 1);

    result = f_opendir(&directory, path);
    if (result != FR_OK) {
        return lua_fs_raise(state, "list", result);
    }

    lua_newtable(state);
    index = 1;

    for (;;) {
        result = f_readdir(&directory, &info);

        if (result != FR_OK) {
            (void)f_closedir(&directory);
            return lua_fs_raise(state, "list", result);
        }

        if (info.fname[0] == '\0') {
            break;
        }

        if (lua_fs_is_dot_entry(info.fname)) {
            continue;
        }

        lua_pushstring(state, info.fname);
        lua_rawseti(state, -2, index);
        index++;
    }

    result = f_closedir(&directory);
    if (result != FR_OK) {
        return lua_fs_raise(state, "list", result);
    }

    return 1;
}

static int lua_fs_list_info(lua_State* state) {
    const char* path;
    DIR directory;
    FILINFO info;
    FRESULT result;
    int index;

    path = lua_fs_check_path(state, 1);

    result = f_opendir(&directory, path);
    if (result != FR_OK) {
        return lua_fs_raise(state, "listInfo", result);
    }

    lua_newtable(state);
    index = 1;

    for (;;) {
        result = f_readdir(&directory, &info);

        if (result != FR_OK) {
            (void)f_closedir(&directory);
            return lua_fs_raise(state, "listInfo", result);
        }

        if (info.fname[0] == '\0') {
            break;
        }

        if (lua_fs_is_dot_entry(info.fname)) {
            continue;
        }

        lua_fs_push_attributes(state, &info);
        lua_pushstring(state, info.fname);
        lua_setfield(state, -2, "name");
        lua_rawseti(state, -2, index);
        index++;
    }

    result = f_closedir(&directory);
    if (result != FR_OK) {
        return lua_fs_raise(state, "listInfo", result);
    }

    return 1;
}


static int lua_fs_exists(lua_State* state) {
    const char* path;
    FILINFO info;
    FRESULT result;

    path = lua_fs_check_path(state, 1);

    if (lua_fs_is_root_path(path)) {
        lua_pushboolean(state, 1);
        return 1;
    }

    result = f_stat(path, &info);
    lua_pushboolean(state, result == FR_OK);
    return 1;
}


static int lua_fs_is_dir(lua_State* state) {
    const char* path;
    FILINFO info;
    FRESULT result;

    path = lua_fs_check_path(state, 1);

    if (lua_fs_is_root_path(path)) {
        lua_pushboolean(state, 1);
        return 1;
    }

    result = f_stat(path, &info);

    lua_pushboolean(
        state,
        result == FR_OK && (info.fattrib & AM_DIR) != 0
    );

    return 1;
}


static int lua_fs_get_size(lua_State* state) {
    const char* path;
    FILINFO info;
    FRESULT result;

    path = lua_fs_check_path(state, 1);

    if (lua_fs_is_root_path(path)) {
        lua_pushinteger(state, 0);
        return 1;
    }

    result = f_stat(path, &info);
    if (result != FR_OK) {
        return lua_fs_raise(state, "getSize", result);
    }

    lua_pushinteger(state, (lua_Integer)info.fsize);
    return 1;
}


static int lua_fs_attributes(lua_State* state) {
    const char* path;
    FILINFO info;
    FRESULT result;

    path = lua_fs_check_path(state, 1);

    if (lua_fs_is_root_path(path)) {
        lua_fs_push_attributes(state, NULL);
        lua_pushboolean(state, 1);
        lua_setfield(state, -2, "is_dir");
        lua_pushboolean(state, 1);
        lua_setfield(state, -2, "isDir");
        return 1;
    }

    result = f_stat(path, &info);
    if (result != FR_OK) {
        return lua_fs_raise(state, "attributes", result);
    }

    lua_fs_push_attributes(state, &info);
    return 1;
}


static int lua_fs_is_read_only(lua_State* state) {
    const char* path;
    FILINFO info;
    FRESULT result;

    path = lua_fs_check_path(state, 1);

    if (lua_fs_is_root_path(path)) {
        lua_pushboolean(state, 0);
        return 1;
    }

    result = f_stat(path, &info);
    if (result != FR_OK) {
        return lua_fs_raise(state, "isReadOnly", result);
    }

    lua_pushboolean(state, (info.fattrib & AM_RDO) != 0);
    return 1;
}


static FRESULT lua_fs_copy_file(const char* source_path, const char* destination_path) {
    FIL source;
    FIL destination;
    FRESULT result;
    UINT bytes_read;
    UINT bytes_written;
    BYTE buffer[LUA_FS_COPY_BUFFER_SIZE];
    int source_open = 0;
    int destination_open = 0;

    result = f_open(&source, source_path, FA_READ | FA_OPEN_EXISTING);
    if (result != FR_OK) {
        return result;
    }

    source_open = 1;

    result = f_open(&destination, destination_path, FA_WRITE | FA_CREATE_NEW);
    if (result != FR_OK) {
        (void)f_close(&source);
        return result;
    }

    destination_open = 1;

    for (;;) {
        bytes_read = 0;
        result = f_read(&source, buffer, (UINT)sizeof(buffer), &bytes_read);
        if (result != FR_OK) {
            break;
        }

        if (bytes_read == 0) {
            break;
        }

        bytes_written = 0;
        result = f_write(&destination, buffer, bytes_read, &bytes_written);
        if (result != FR_OK) {
            break;
        }

        if (bytes_written != bytes_read) {
            result = FR_DISK_ERR;
            break;
        }
    }

    if (destination_open) {
        FRESULT close_result = f_close(&destination);
        if (result == FR_OK && close_result != FR_OK) {
            result = close_result;
        }
    }

    if (source_open) {
        FRESULT close_result = f_close(&source);
        if (result == FR_OK && close_result != FR_OK) {
            result = close_result;
        }
    }

    return result;
}


static FRESULT lua_fs_copy_tree(
    const char* source_path,
    const char* destination_path,
    int depth
) {
    FILINFO info;
    FRESULT result;

    if (depth > LUA_FS_MAX_RECURSION) {
        return FR_NOT_ENOUGH_CORE;
    }

    result = f_stat(source_path, &info);
    if (result != FR_OK) {
        return result;
    }

    if ((info.fattrib & AM_DIR) == 0) {
        return lua_fs_copy_file(source_path, destination_path);
    }

    result = f_mkdir(destination_path);
    if (result != FR_OK) {
        return result;
    }

    {
        DIR directory;
        FILINFO child_info;

        result = f_opendir(&directory, source_path);
        if (result != FR_OK) {
            (void)f_unlink(destination_path);
            return result;
        }

        for (;;) {
            char child_source[LUA_FS_PATH_CAPACITY];
            char child_destination[LUA_FS_PATH_CAPACITY];

            result = f_readdir(&directory, &child_info);
            if (result != FR_OK) {
                break;
            }

            if (child_info.fname[0] == '\0') {
                break;
            }

            if (lua_fs_is_dot_entry(child_info.fname)) {
                continue;
            }

            if (
                !lua_fs_join_path(
                    child_source,
                    sizeof(child_source),
                    source_path,
                    child_info.fname
                ) ||
                !lua_fs_join_path(
                    child_destination,
                    sizeof(child_destination),
                    destination_path,
                    child_info.fname
                )
            ) {
                result = FR_INVALID_NAME;
                break;
            }

            result = lua_fs_copy_tree(
                child_source,
                child_destination,
                depth + 1
            );

            if (result != FR_OK) {
                break;
            }
        }

        {
            FRESULT close_result = f_closedir(&directory);
            if (result == FR_OK && close_result != FR_OK) {
                result = close_result;
            }
        }
    }

    return result;
}


static FRESULT lua_fs_delete_tree(const char* path, int depth) {
    FILINFO info;
    FRESULT result;

    if (depth > LUA_FS_MAX_RECURSION) {
        return FR_NOT_ENOUGH_CORE;
    }

    result = f_stat(path, &info);
    if (result != FR_OK) {
        return result;
    }

    if ((info.fattrib & AM_DIR) == 0) {
        return f_unlink(path);
    }

    {
        DIR directory;
        FILINFO child_info;

        result = f_opendir(&directory, path);
        if (result != FR_OK) {
            return result;
        }

        for (;;) {
            char child_path[LUA_FS_PATH_CAPACITY];

            result = f_readdir(&directory, &child_info);
            if (result != FR_OK) {
                break;
            }

            if (child_info.fname[0] == '\0') {
                break;
            }

            if (lua_fs_is_dot_entry(child_info.fname)) {
                continue;
            }

            if (!lua_fs_join_path(
                child_path,
                sizeof(child_path),
                path,
                child_info.fname
            )) {
                result = FR_INVALID_NAME;
                break;
            }

            result = lua_fs_delete_tree(child_path, depth + 1);
            if (result != FR_OK) {
                break;
            }
        }

        {
            FRESULT close_result = f_closedir(&directory);
            if (result == FR_OK && close_result != FR_OK) {
                result = close_result;
            }
        }
    }

    if (result != FR_OK) {
        return result;
    }

    return f_unlink(path);
}


static int lua_fs_make_dir(lua_State* state) {
    const char* path;
    FRESULT result;

    path = lua_fs_check_path(state, 1);

    result = f_mkdir(path);
    if (result != FR_OK) {
        return lua_fs_raise(state, "makeDir", result);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_fs_delete(lua_State* state) {
    const char* path;
    FRESULT result;

    path = lua_fs_check_path(state, 1);

    if (lua_fs_is_root_path(path)) {
        return luaL_error(state, "fs.delete refuses to delete a volume root");
    }

    result = lua_fs_delete_tree(path, 0);
    if (result != FR_OK) {
        return lua_fs_raise(state, "delete", result);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_fs_copy(lua_State* state) {
    const char* source_path;
    const char* destination_path;
    FRESULT result;

    source_path = lua_fs_check_path(state, 1);
    destination_path = lua_fs_check_path(state, 2);

    if (lua_fs_paths_equal(source_path, destination_path)) {
        return luaL_error(state, "fs.copy source and destination are the same path");
    }

    if (lua_fs_path_is_child_of(source_path, destination_path)) {
        return luaL_error(state, "fs.copy destination must not be inside the source directory");
    }

    result = lua_fs_copy_tree(source_path, destination_path, 0);
    if (result != FR_OK) {
        return lua_fs_raise(state, "copy", result);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_fs_move(lua_State* state) {
    const char* source_path;
    const char* destination_path;
    FRESULT result;

    source_path = lua_fs_check_path(state, 1);
    destination_path = lua_fs_check_path(state, 2);

    if (lua_fs_is_root_path(source_path)) {
        return luaL_error(state, "fs.move refuses to move a volume root");
    }

    if (lua_fs_paths_equal(source_path, destination_path)) {
        return luaL_error(state, "fs.move source and destination are the same path");
    }

    if (lua_fs_path_is_child_of(source_path, destination_path)) {
        return luaL_error(state, "fs.move destination must not be inside the source directory");
    }

    result = f_rename(source_path, destination_path);
    if (result != FR_OK) {
        return lua_fs_raise(state, "move", result);
    }

    lua_pushboolean(state, 1);
    return 1;
}


static int lua_fs_get_name(lua_State* state) {
    const char* path;
    size_t length;
    size_t end;
    size_t start;

    path = lua_fs_check_path(state, 1);
    length = lua_fs_strlen(path);

    while (length > 0 && lua_fs_is_separator(path[length - 1])) {
        length--;
    }

    if (length == 0 || (length == 2 && path[1] == ':')) {
        lua_pushliteral(state, "");
        return 1;
    }

    end = length;
    start = end;

    while (start > 0 && !lua_fs_is_separator(path[start - 1]) && path[start - 1] != ':') {
        start--;
    }

    lua_pushlstring(state, path + start, end - start);
    return 1;
}


static int lua_fs_get_dir(lua_State* state) {
    const char* path;
    size_t length;
    size_t end;
    size_t index;

    path = lua_fs_check_path(state, 1);
    length = lua_fs_strlen(path);

    while (length > 0 && lua_fs_is_separator(path[length - 1])) {
        length--;
    }

    if (length == 0) {
        lua_pushliteral(state, "");
        return 1;
    }

    index = length;
    while (index > 0 && !lua_fs_is_separator(path[index - 1])) {
        index--;
    }

    if (index == 0) {
        lua_pushliteral(state, "");
        return 1;
    }

    end = index - 1;

    if (end == 2 && path[1] == ':') {
        lua_pushlstring(state, path, 3);
        return 1;
    }

    while (end > 0 && lua_fs_is_separator(path[end - 1])) {
        end--;
    }

    if (end == 0 && lua_fs_is_separator(path[0])) {
        lua_pushliteral(state, "/");
        return 1;
    }

    lua_pushlstring(state, path, end);
    return 1;
}


static int lua_fs_combine(lua_State* state) {
    int argument_count;
    char result[LUA_FS_PATH_CAPACITY];
    size_t result_length;

    argument_count = lua_gettop(state);
    result[0] = '\0';
    result_length = 0;

    for (int argument_index = 1; argument_index <= argument_count; argument_index++) {
        const char* part;
        size_t part_length;
        size_t part_start;
        int is_absolute_drive_path;

        part = luaL_checklstring(state, argument_index, &part_length);

        if (lua_fs_has_embedded_nul(part, part_length)) {
            return luaL_argerror(state, argument_index, "path must not contain a NUL byte");
        }

        if (part_length == 0) {
            continue;
        }

        is_absolute_drive_path =
            part_length >= 2 &&
            part[1] == ':';

        if (is_absolute_drive_path) {
            if (part_length >= LUA_FS_PATH_CAPACITY) {
                return luaL_argerror(state, argument_index, "path is too long");
            }

            result_length = 0;
            for (size_t index = 0; index < part_length; index++) {
                result[result_length++] = part[index] == '\\' ? '/' : part[index];
            }
            result[result_length] = '\0';
            continue;
        }

        part_start = 0;
        while (part_start < part_length && lua_fs_is_separator(part[part_start])) {
            part_start++;
        }

        if (part_start == part_length) {
            if (result_length == 0) {
                result[0] = '/';
                result[1] = '\0';
                result_length = 1;
            }
            continue;
        }

        if (
            result_length > 0 &&
            !lua_fs_is_separator(result[result_length - 1]) &&
            result[result_length - 1] != ':'
        ) {
            if (result_length + 1 >= sizeof(result)) {
                return luaL_error(state, "fs.combine result is too long");
            }

            result[result_length++] = '/';
        }

        if (result_length + (part_length - part_start) >= sizeof(result)) {
            return luaL_error(state, "fs.combine result is too long");
        }

        for (size_t index = part_start; index < part_length; index++) {
            result[result_length++] = part[index] == '\\' ? '/' : part[index];
        }

        result[result_length] = '\0';
    }

    lua_pushlstring(state, result, result_length);
    return 1;
}


static int lua_fs_get_free_space(lua_State* state) {
    const char* path;
    DWORD free_clusters;
    FATFS* filesystem;
    FRESULT result;
    uint64_t bytes;

    path = lua_fs_check_path(state, 1);
    free_clusters = 0;
    filesystem = NULL;

    result = f_getfree(path, &free_clusters, &filesystem);
    if (result != FR_OK || filesystem == NULL) {
        return lua_fs_raise(state, "getFreeSpace", result == FR_OK ? FR_INT_ERR : result);
    }

    bytes = (uint64_t)free_clusters * (uint64_t)filesystem->csize * (uint64_t)FF_MAX_SS;
    lua_pushinteger(state, (lua_Integer)bytes);
    return 1;
}


static int lua_fs_get_capacity(lua_State* state) {
    const char* path;
    DWORD free_clusters;
    FATFS* filesystem;
    FRESULT result;
    uint64_t bytes;

    path = lua_fs_check_path(state, 1);
    free_clusters = 0;
    filesystem = NULL;

    result = f_getfree(path, &free_clusters, &filesystem);
    if (result != FR_OK || filesystem == NULL) {
        return lua_fs_raise(state, "getCapacity", result == FR_OK ? FR_INT_ERR : result);
    }

    bytes =
        (uint64_t)(filesystem->n_fatent - 2U) *
        (uint64_t)filesystem->csize *
        (uint64_t)FF_MAX_SS;

    lua_pushinteger(state, (lua_Integer)bytes);
    return 1;
}


static const luaL_Reg lua_fs_functions[] = {
    { "list",         lua_fs_list },
    { "listInfo",     lua_fs_list_info },
    { "exists",       lua_fs_exists },
    { "isDir",        lua_fs_is_dir },
    { "getSize",      lua_fs_get_size },
    { "attributes",   lua_fs_attributes },
    { "isReadOnly",   lua_fs_is_read_only },
    { "makeDir",      lua_fs_make_dir },
    { "delete",       lua_fs_delete },
    { "copy",         lua_fs_copy },
    { "move",         lua_fs_move },
    { "rename",       lua_fs_move },
    { "getName",      lua_fs_get_name },
    { "getDir",       lua_fs_get_dir },
    { "combine",      lua_fs_combine },
    { "getFreeSpace", lua_fs_get_free_space },
    { "getCapacity",  lua_fs_get_capacity },
    { NULL,             NULL }
};


int luaopen_fs(lua_State* state) {
    luaL_newlib(state, lua_fs_functions);
    return 1;
}
