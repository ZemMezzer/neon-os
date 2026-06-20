#include "lua_shell.h"

#include <stddef.h>

#include "console.h"
#include "ff.h"
#include "stdio.h"
#include "lua_runner.h"
#include "shell_commands.h"

#define LUA_SHELL_PATH_MAX 512
#define LUA_SHELL_PATH_FILE "0:/system/variables/PATH.txt"
#define LUA_SHELL_PATH_MAX_ENTRIES 16
#define LUA_SHELL_DEFAULT_PATH "./system\n./programs\n"

typedef struct LuaShellPathList {
    char raw[LUA_SHELL_PATH_MAX_ENTRIES][LUA_SHELL_PATH_MAX];
    char absolute[LUA_SHELL_PATH_MAX_ENTRIES][LUA_SHELL_PATH_MAX];
    int count;
} LuaShellPathList;


static int lua_shell_strlen(const char* text) {
    int length = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}


static int lua_shell_equals(const char* left, const char* right) {
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


static int lua_shell_lower(int value) {
    if (value >= 'A' && value <= 'Z') {
        return value - 'A' + 'a';
    }

    return value;
}


static int lua_shell_ends_with_lua(const char* text) {
    int length = lua_shell_strlen(text);

    return
        length >= 4 &&
        text[length - 4] == '.' &&
        lua_shell_lower(text[length - 3]) == 'l' &&
        lua_shell_lower(text[length - 2]) == 'u' &&
        lua_shell_lower(text[length - 1]) == 'a';
}


static void lua_shell_copy(char* destination, int capacity, const char* source) {
    int index = 0;

    if (destination == NULL || capacity <= 0) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    while (source[index] != '\0' && index + 1 < capacity) {
        destination[index] = source[index];
        index++;
    }

    destination[index] = '\0';
}


static int lua_shell_append_char(
    char* destination,
    int capacity,
    int* position,
    char value
) {
    if (
        destination == NULL ||
        position == NULL ||
        capacity <= 0 ||
        *position < 0 ||
        *position + 1 >= capacity
    ) {
        return -1;
    }

    if (value == '\\') {
        value = '/';
    }

    destination[*position] = value;
    *position += 1;
    destination[*position] = '\0';

    return 0;
}


static int lua_shell_append_text(
    char* destination,
    int capacity,
    int* position,
    const char* source
) {
    int index = 0;

    if (source == NULL) {
        return 0;
    }

    while (source[index] != '\0') {
        if (lua_shell_append_char(destination, capacity, position, source[index]) != 0) {
            return -1;
        }

        index++;
    }

    return 0;
}


static void lua_shell_trim(char* text) {
    int start = 0;
    int end;
    int index;

    if (text == NULL) {
        return;
    }

    while (
        text[start] == ' ' ||
        text[start] == '\t' ||
        text[start] == '\r' ||
        text[start] == '\n'
    ) {
        start++;
    }

    end = lua_shell_strlen(text);

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

        end -= start;
    }

    text[end] = '\0';
}


static void lua_shell_strip_trailing_slash(char* path) {
    int length = lua_shell_strlen(path);

    while (length > 3 && path[length - 1] == '/') {
        path[length - 1] = '\0';
        length--;
    }
}


static int lua_shell_path_entry_to_absolute(
    const char* entry,
    char* output,
    int output_capacity
) {
    int position = 0;
    int index = 0;

    if (
        entry == NULL ||
        output == NULL ||
        output_capacity <= 0 ||
        entry[0] == '\0'
    ) {
        return -1;
    }

    output[0] = '\0';

    if (entry[0] >= '0' && entry[0] <= '9' && entry[1] == ':') {
        if (lua_shell_append_text(output, output_capacity, &position, entry) != 0) {
            return -1;
        }
    } else {
        if (
            lua_shell_append_char(output, output_capacity, &position, '0') != 0 ||
            lua_shell_append_char(output, output_capacity, &position, ':') != 0 ||
            lua_shell_append_char(output, output_capacity, &position, '/') != 0
        ) {
            return -1;
        }

        if (entry[0] == '.' && (entry[1] == '/' || entry[1] == '\\')) {
            index = 2;
        } else if (entry[0] == '/' || entry[0] == '\\') {
            index = 1;
        }

        while (entry[index] != '\0') {
            if (lua_shell_append_char(output, output_capacity, &position, entry[index]) != 0) {
                return -1;
            }

            index++;
        }
    }

    lua_shell_strip_trailing_slash(output);
    return 0;
}


static int lua_shell_absolute_to_entry(
    const char* absolute,
    char* output,
    int output_capacity
) {
    int position = 0;
    int index;

    if (
        absolute == NULL ||
        output == NULL ||
        output_capacity <= 0 ||
        absolute[0] == '\0'
    ) {
        return -1;
    }

    output[0] = '\0';

    if (
        absolute[0] == '0' &&
        absolute[1] == ':' &&
        absolute[2] == '/'
    ) {
        if (
            lua_shell_append_char(output, output_capacity, &position, '.') != 0 ||
            lua_shell_append_char(output, output_capacity, &position, '/') != 0
        ) {
            return -1;
        }

        for (index = 3; absolute[index] != '\0'; index++) {
            if (lua_shell_append_char(output, output_capacity, &position, absolute[index]) != 0) {
                return -1;
            }
        }

        return 0;
    }

    lua_shell_copy(output, output_capacity, absolute);
    return 0;
}


static int lua_shell_join(
    const char* directory,
    const char* name,
    char* output,
    int output_capacity
) {
    int position = 0;
    int length;

    if (directory == NULL || name == NULL || output == NULL || output_capacity <= 0) {
        return -1;
    }

    output[0] = '\0';

    if (lua_shell_append_text(output, output_capacity, &position, directory) != 0) {
        return -1;
    }

    length = lua_shell_strlen(output);

    if (length > 0 && output[length - 1] != '/') {
        if (lua_shell_append_char(output, output_capacity, &position, '/') != 0) {
            return -1;
        }
    }

    return lua_shell_append_text(output, output_capacity, &position, name);
}


static int lua_shell_directory_exists(const char* path) {
    FILINFO info;
    FRESULT result = f_stat(path, &info);

    return result == FR_OK && (info.fattrib & AM_DIR) != 0;
}


static int lua_shell_ensure_directory(const char* path) {
    FILINFO info;
    FRESULT result = f_stat(path, &info);

    if (result == FR_OK) {
        return (info.fattrib & AM_DIR) != 0 ? 0 : -1;
    }

    if (result != FR_NO_FILE && result != FR_NO_PATH) {
        return -1;
    }

    result = f_mkdir(path);

    if (result != FR_OK && result != FR_EXIST) {
        return -1;
    }

    result = f_stat(path, &info);

    return result == FR_OK && (info.fattrib & AM_DIR) != 0 ? 0 : -1;
}


static int lua_shell_write_path_text(const char* text) {
    FILE* file;

    if (text == NULL) {
        return -1;
    }

    file = fopen(LUA_SHELL_PATH_FILE, "w");

    if (file == NULL) {
        return -1;
    }

    if (fputs(text, file) == EOF || fclose(file) == EOF) {
        return -1;
    }

    return 0;
}


static int lua_shell_ensure_path_file(void) {
    FILINFO info;
    FRESULT result = f_stat(LUA_SHELL_PATH_FILE, &info);

    if (result == FR_OK) {
        return (info.fattrib & AM_DIR) == 0 ? 0 : -1;
    }

    if (result != FR_NO_FILE && result != FR_NO_PATH) {
        return -1;
    }

    if (
        lua_shell_ensure_directory("0:/system") != 0 ||
        lua_shell_ensure_directory("0:/system/variables") != 0
    ) {
        return -1;
    }

    return lua_shell_write_path_text(LUA_SHELL_DEFAULT_PATH);
}


static int lua_shell_load_path_list(LuaShellPathList* list) {
    FILE* file;
    char line[LUA_SHELL_PATH_MAX];

    if (list == NULL) {
        return -1;
    }

    list->count = 0;

    if (lua_shell_ensure_path_file() != 0) {
        return -1;
    }

    file = fopen(LUA_SHELL_PATH_FILE, "r");

    if (file == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        lua_shell_trim(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (list->count >= LUA_SHELL_PATH_MAX_ENTRIES) {
            (void)fclose(file);
            return -1;
        }

        if (
            lua_shell_path_entry_to_absolute(
                line,
                list->absolute[list->count],
                sizeof(list->absolute[list->count])
            ) != 0
        ) {
            (void)fclose(file);
            return -1;
        }

        lua_shell_copy(
            list->raw[list->count],
            sizeof(list->raw[list->count]),
            line
        );

        list->count++;
    }

    return fclose(file) == EOF ? -1 : 0;
}


static int lua_shell_write_path_list(const LuaShellPathList* list) {
    FILE* file;

    if (list == NULL) {
        return -1;
    }

    file = fopen(LUA_SHELL_PATH_FILE, "w");

    if (file == NULL) {
        return -1;
    }

    for (int index = 0; index < list->count; index++) {
        if (
            fputs(list->raw[index], file) == EOF ||
            fputc('\n', file) == EOF
        ) {
            (void)fclose(file);
            return -1;
        }
    }

    return fclose(file) == EOF ? -1 : 0;
}


static int lua_shell_command_name_is_safe(const char* name) {
    int index = 0;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    while (name[index] != '\0') {
        char character = name[index];

        if (
            character == '/' ||
            character == '\\' ||
            character == ':' ||
            character == ' ' ||
            character == '\t'
        ) {
            return 0;
        }

        index++;
    }

    return 1;
}


static int lua_shell_find_program(
    const char* command,
    char* output,
    int output_capacity
) {
    LuaShellPathList paths;
    char filename[LUA_SHELL_PATH_MAX];
    int position = 0;

    if (
        command == NULL ||
        output == NULL ||
        output_capacity <= 0 ||
        !lua_shell_command_name_is_safe(command)
    ) {
        return 0;
    }

    filename[0] = '\0';

    if (lua_shell_append_text(filename, sizeof(filename), &position, command) != 0) {
        return 0;
    }

    if (!lua_shell_ends_with_lua(filename)) {
        if (lua_shell_append_text(filename, sizeof(filename), &position, ".lua") != 0) {
            return 0;
        }
    }

    if (lua_shell_load_path_list(&paths) != 0) {
        return 0;
    }

    for (int index = 0; index < paths.count; index++) {
        FILINFO info;
        FRESULT result;
        char candidate[LUA_SHELL_PATH_MAX];

        if (!lua_shell_directory_exists(paths.absolute[index])) {
            continue;
        }

        if (
            lua_shell_join(
                paths.absolute[index],
                filename,
                candidate,
                sizeof(candidate)
            ) != 0
        ) {
            continue;
        }

        result = f_stat(candidate, &info);

        if (result == FR_OK && (info.fattrib & AM_DIR) == 0) {
            lua_shell_copy(output, output_capacity, candidate);
            return 1;
        }
    }

    return 0;
}


static int lua_shell_run(
    const char* path,
    int argument_count,
    char** arguments
) {
    int status = lua_run_file_args(path, argument_count, arguments);

    return status < 0 ? 1 : status;
}


static int cmd_lua(int argc, char** argv) {
    char path[LUA_SHELL_PATH_MAX];

    if (argc < 2) {
        console_write("usage: lua <script> [arguments...]\n");
        return 1;
    }

    if (shell_resolve_path(argv[1], path, sizeof(path)) != 0) {
        console_write("lua: path too long\n");
        return 1;
    }

    return lua_shell_run(path, argc - 2, argv + 2);
}


static int cmd_path(int argc, char** argv) {
    LuaShellPathList paths;

    if (lua_shell_load_path_list(&paths) != 0) {
        console_write("path: cannot read " LUA_SHELL_PATH_FILE "\n");
        return 1;
    }

    if (argc == 1) {
        console_write("PATH:\n");

        for (int index = 0; index < paths.count; index++) {
            console_write("  ");
            console_write(paths.raw[index]);
            console_write("\n");
        }

        return 0;
    }

    if (lua_shell_equals(argv[1], "reset")) {
        if (argc != 2) {
            console_write("usage: path reset\n");
            return 1;
        }

        if (lua_shell_write_path_text(LUA_SHELL_DEFAULT_PATH) != 0) {
            console_write("path: reset failed\n");
            return 1;
        }

        console_write("PATH reset\n");
        return 0;
    }

    if (lua_shell_equals(argv[1], "add")) {
        char absolute[LUA_SHELL_PATH_MAX];
        char stored[LUA_SHELL_PATH_MAX];

        if (argc != 3) {
            console_write("usage: path add <directory>\n");
            return 1;
        }

        if (
            lua_shell_path_entry_to_absolute(argv[2], absolute, sizeof(absolute)) != 0 ||
            !lua_shell_directory_exists(absolute)
        ) {
            console_write("path: directory not found\n");
            return 1;
        }

        for (int index = 0; index < paths.count; index++) {
            if (lua_shell_equals(paths.absolute[index], absolute)) {
                console_write("path: already present\n");
                return 0;
            }
        }

        if (paths.count >= LUA_SHELL_PATH_MAX_ENTRIES) {
            console_write("path: too many entries\n");
            return 1;
        }

        if (lua_shell_absolute_to_entry(absolute, stored, sizeof(stored)) != 0) {
            console_write("path: path too long\n");
            return 1;
        }

        lua_shell_copy(
            paths.raw[paths.count],
            sizeof(paths.raw[paths.count]),
            stored
        );
        lua_shell_copy(
            paths.absolute[paths.count],
            sizeof(paths.absolute[paths.count]),
            absolute
        );
        paths.count++;

        if (lua_shell_write_path_list(&paths) != 0) {
            console_write("path: write failed\n");
            return 1;
        }

        console_write("path: added ");
        console_write(stored);
        console_write("\n");
        return 0;
    }

    if (lua_shell_equals(argv[1], "remove")) {
        char absolute[LUA_SHELL_PATH_MAX];
        int found = 0;
        int write_index = 0;

        if (argc != 3) {
            console_write("usage: path remove <directory>\n");
            return 1;
        }

        if (lua_shell_path_entry_to_absolute(argv[2], absolute, sizeof(absolute)) != 0) {
            console_write("path: invalid directory\n");
            return 1;
        }

        for (int index = 0; index < paths.count; index++) {
            if (lua_shell_equals(paths.absolute[index], absolute)) {
                found = 1;
                continue;
            }

            if (write_index != index) {
                lua_shell_copy(
                    paths.raw[write_index],
                    sizeof(paths.raw[write_index]),
                    paths.raw[index]
                );
                lua_shell_copy(
                    paths.absolute[write_index],
                    sizeof(paths.absolute[write_index]),
                    paths.absolute[index]
                );
            }

            write_index++;
        }

        if (!found) {
            console_write("path: entry not found\n");
            return 1;
        }

        paths.count = write_index;

        if (lua_shell_write_path_list(&paths) != 0) {
            console_write("path: write failed\n");
            return 1;
        }

        console_write("path: removed\n");
        return 0;
    }

    console_write("usage: path [add <directory> | remove <directory> | reset]\n");
    return 1;
}


static int lua_shell_command_fallback(
    int argc,
    char** argv,
    int* out_status
) {
    char path[LUA_SHELL_PATH_MAX];

    if (argc <= 0 || argv == NULL || out_status == NULL) {
        return 0;
    }

    if (!lua_shell_find_program(argv[0], path, sizeof(path))) {
        return 0;
    }

    *out_status = lua_shell_run(path, argc - 1, argv + 1);
    return 1;
}


void lua_shell_register_commands(void) {
    if (lua_shell_ensure_path_file() != 0) {
        console_write("lua: cannot initialize PATH.txt\n");
    }

    (void)shell_register_command(
        "lua",
        "run a Lua script",
        cmd_lua
    );

    (void)shell_register_command(
        "path",
        "show or edit Lua program search path",
        cmd_path
    );

    (void)shell_set_command_fallback(lua_shell_command_fallback);
}
