#include "shell_commands.h"

#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "stdio.h"
#include "ff.h"

#define SHELL_MAX_ARGS 16
#define SHELL_LINE_MAX 256
#define SHELL_PATH_MAX 512

typedef int (*ShellCommandFn)(int argc, char** argv);

typedef struct ShellCommand {
    const char* name;
    const char* help;
    ShellCommandFn fn;
} ShellCommand;

static char shell_cwd[SHELL_PATH_MAX] = "0:/";

static int shell_str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static int shell_str_len(const char* s) {
    int len = 0;

    while (s && s[len]) {
        len++;
    }

    return len;
}

static void shell_copy(char* dst, int dst_size, const char* src) {
    int i = 0;

    if (!dst || dst_size <= 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static int shell_append_char(char* dst, int dst_size, int* pos, char ch) {
    if (*pos + 1 >= dst_size) {
        return -1;
    }

    if (ch == '\\') {
        ch = '/';
    }

    dst[*pos] = ch;
    *pos = *pos + 1;
    dst[*pos] = 0;

    return 0;
}

static void shell_print_error(const char* text) {
    console_write("Error: ");
    console_write(text);
    console_write("\n");
}

static int shell_make_path(const char* input, char* out, int out_size) {
    int pos = 0;
    int i = 0;
    int cwd_len;

    if (!input || !out || out_size <= 0) {
        return -1;
    }

    out[0] = 0;

    if (input[0] == 0) {
        shell_copy(out, out_size, shell_cwd);
        return 0;
    }

    /*
        FatFs absolute path:
        0:/Folder/File.txt
    */
    if (input[0] >= '0' && input[0] <= '9' && input[1] == ':') {
        while (input[i]) {
            if (shell_append_char(out, out_size, &pos, input[i]) != 0) {
                return -1;
            }

            i++;
        }

        return 0;
    }

    /*
        Unix-style absolute path:
        /Folder/File.txt -> 0:/Folder/File.txt
    */
    if (input[0] == '/' || input[0] == '\\') {
        if (shell_append_char(out, out_size, &pos, '0') != 0) {
            return -1;
        }

        if (shell_append_char(out, out_size, &pos, ':') != 0) {
            return -1;
        }

        while (input[i]) {
            if (shell_append_char(out, out_size, &pos, input[i]) != 0) {
                return -1;
            }

            i++;
        }

        return 0;
    }

    /*
        Relative path:
        file.txt -> cwd/file.txt
    */
    cwd_len = shell_str_len(shell_cwd);

    for (i = 0; i < cwd_len; i++) {
        if (shell_append_char(out, out_size, &pos, shell_cwd[i]) != 0) {
            return -1;
        }
    }

    if (pos > 0 && out[pos - 1] != '/') {
        if (shell_append_char(out, out_size, &pos, '/') != 0) {
            return -1;
        }
    }

    i = 0;

    while (input[i]) {
        if (shell_append_char(out, out_size, &pos, input[i]) != 0) {
            return -1;
        }

        i++;
    }

    return 0;
}

static void shell_path_parent(char* path) {
    int len;
    int i;

    if (!path) {
        return;
    }

    if (shell_str_equal(path, "0:/")) {
        return;
    }

    len = shell_str_len(path);

    while (len > 3 && path[len - 1] == '/') {
        path[len - 1] = 0;
        len--;
    }

    for (i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            if (i <= 2) {
                path[0] = '0';
                path[1] = ':';
                path[2] = '/';
                path[3] = 0;
            } else {
                path[i] = 0;
            }

            return;
        }
    }
}

static int shell_parse_line(const char* line, char* work, int work_size, char** argv) {
    int argc = 0;
    int wi = 0;
    int in_quote = 0;
    int token_started = 0;

    if (!line || !work || !argv) {
        return 0;
    }

    for (int i = 0; line[i] != 0; i++) {
        char ch = line[i];

        if (ch == '"') {
            if (!token_started) {
                if (argc >= SHELL_MAX_ARGS) {
                    break;
                }

                argv[argc++] = &work[wi];
                token_started = 1;
            }

            in_quote = !in_quote;
            continue;
        }

        if (!in_quote && (ch == ' ' || ch == '\t')) {
            if (token_started) {
                if (wi + 1 >= work_size) {
                    break;
                }

                work[wi++] = 0;
                token_started = 0;
            }

            continue;
        }

        if (!token_started) {
            if (argc >= SHELL_MAX_ARGS) {
                break;
            }

            argv[argc++] = &work[wi];
            token_started = 1;
        }

        if (wi + 1 >= work_size) {
            break;
        }

        work[wi++] = ch;
    }

    if (token_started && wi < work_size) {
        work[wi++] = 0;
    }

    if (wi < work_size) {
        work[wi] = 0;
    }

    return argc;
}

static int cmd_help(int argc, char** argv);
static int cmd_pwd(int argc, char** argv);
static int cmd_cd(int argc, char** argv);
static int cmd_ls(int argc, char** argv);
static int cmd_mkdir(int argc, char** argv);
static int cmd_cat(int argc, char** argv);
static int cmd_write(int argc, char** argv);
static int cmd_append(int argc, char** argv);
static int cmd_rm(int argc, char** argv);
static int cmd_mv(int argc, char** argv);
static int cmd_echo(int argc, char** argv);

static const ShellCommand commands[] = {
    { "help",   "show commands",              cmd_help },
    { "pwd",    "show current directory",     cmd_pwd },
    { "cd",     "change directory",           cmd_cd },
    { "ls",     "list directory",             cmd_ls },
    { "mkdir",  "create directory",           cmd_mkdir },
    { "cat",    "print file",                 cmd_cat },
    { "write",  "write text to file",         cmd_write },
    { "append", "append text to file",        cmd_append },
    { "rm",     "remove file or empty dir",   cmd_rm },
    { "mv",     "rename or move",             cmd_mv },
    { "echo",   "print text",                 cmd_echo },
};

static const int command_count = sizeof(commands) / sizeof(commands[0]);

static int cmd_help(int argc, char** argv) {
    (void)argc;
    (void)argv;

    console_write("Commands:\n");

    for (int i = 0; i < command_count; i++) {
        console_write("  ");
        console_write(commands[i].name);
        console_write(" - ");
        console_write(commands[i].help);
        console_write("\n");
    }

    return 0;
}

static int cmd_pwd(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (shell_str_equal(shell_cwd, "0:/")) {
        console_write("/\n");
        return 0;
    }

    if (shell_cwd[0] == '0' && shell_cwd[1] == ':') {
        console_write(shell_cwd + 2);
        console_write("\n");
        return 0;
    }

    console_write(shell_cwd);
    console_write("\n");

    return 0;
}

static int cmd_cd(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    DIR dir;
    FRESULT res;

    if (argc < 2) {
        shell_copy(shell_cwd, sizeof(shell_cwd), "0:/");
        return 0;
    }

    if (shell_str_equal(argv[1], "..")) {
        shell_path_parent(shell_cwd);
        return 0;
    }

    if (shell_make_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    res = f_opendir(&dir, path);

    if (res != FR_OK) {
        shell_print_error("directory not found");
        return -1;
    }

    f_closedir(&dir);

    shell_copy(shell_cwd, sizeof(shell_cwd), path);

    return 0;
}

static int cmd_ls(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    DIR dir;
    FILINFO info;
    FRESULT res;

    if (argc >= 2) {
        if (shell_make_path(argv[1], path, sizeof(path)) != 0) {
            shell_print_error("path too long");
            return -1;
        }
    } else {
        shell_copy(path, sizeof(path), shell_cwd);
    }

    res = f_opendir(&dir, path);

    if (res != FR_OK) {
        shell_print_error("cannot open directory");
        return -1;
    }

    while (1) {
        res = f_readdir(&dir, &info);

        if (res != FR_OK) {
            f_closedir(&dir);
            shell_print_error("cannot read directory");
            return -1;
        }

        if (info.fname[0] == 0) {
            break;
        }

        if (info.fattrib & AM_DIR) {
            console_write("[D] ");
        } else {
            console_write("    ");
        }

        console_write(info.fname);
        console_write("\n");
    }

    f_closedir(&dir);

    return 0;
}

static int cmd_mkdir(int argc, char** argv) {
    char path[SHELL_PATH_MAX];

    if (argc < 2) {
        shell_print_error("usage: mkdir <path>");
        return -1;
    }

    if (shell_make_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    if (f_mkdir(path) != FR_OK) {
        shell_print_error("mkdir failed");
        return -1;
    }

    return 0;
}

static int cmd_cat(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    FILE* f;
    char buffer[129];
    size_t n;

    if (argc < 2) {
        shell_print_error("usage: cat <file>");
        return -1;
    }

    if (shell_make_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    f = fopen(path, "r");

    if (!f) {
        shell_print_error("cannot open file");
        return -1;
    }

    while (1) {
        n = fread(buffer, 1, sizeof(buffer) - 1, f);

        if (n == 0) {
            break;
        }

        buffer[n] = 0;
        console_write(buffer);
    }

    fclose(f);
    console_write("\n");

    return 0;
}

static int cmd_write(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    FILE* f;

    if (argc < 3) {
        shell_print_error("usage: write <file> <text>");
        return -1;
    }

    if (shell_make_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    f = fopen(path, "w");

    if (!f) {
        shell_print_error("cannot open file");
        return -1;
    }

    fwrite(argv[2], 1, shell_str_len(argv[2]), f);
    fwrite("\n", 1, 1, f);

    fclose(f);

    return 0;
}

static int cmd_append(int argc, char** argv) {
    char path[SHELL_PATH_MAX];
    FILE* f;

    if (argc < 3) {
        shell_print_error("usage: append <file> <text>");
        return -1;
    }

    if (shell_make_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    f = fopen(path, "a");

    if (!f) {
        shell_print_error("cannot open file");
        return -1;
    }

    fwrite(argv[2], 1, shell_str_len(argv[2]), f);
    fwrite("\n", 1, 1, f);

    fclose(f);

    return 0;
}

static int cmd_rm(int argc, char** argv) {
    char path[SHELL_PATH_MAX];

    if (argc < 2) {
        shell_print_error("usage: rm <path>");
        return -1;
    }

    if (shell_make_path(argv[1], path, sizeof(path)) != 0) {
        shell_print_error("path too long");
        return -1;
    }

    if (remove(path) != 0) {
        shell_print_error("remove failed");
        return -1;
    }

    return 0;
}

static int cmd_mv(int argc, char** argv) {
    char old_path[SHELL_PATH_MAX];
    char new_path[SHELL_PATH_MAX];

    if (argc < 3) {
        shell_print_error("usage: mv <old> <new>");
        return -1;
    }

    if (shell_make_path(argv[1], old_path, sizeof(old_path)) != 0) {
        shell_print_error("old path too long");
        return -1;
    }

    if (shell_make_path(argv[2], new_path, sizeof(new_path)) != 0) {
        shell_print_error("new path too long");
        return -1;
    }

    if (rename(old_path, new_path) != 0) {
        shell_print_error("rename failed");
        return -1;
    }

    return 0;
}

static int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            console_write(" ");
        }

        console_write(argv[i]);
    }

    console_write("\n");

    return 0;
}

void shell_commands_execute(const char* line) {
    char work[SHELL_LINE_MAX];
    char* argv[SHELL_MAX_ARGS];
    int argc;

    if (!line || line[0] == 0) {
        return;
    }

    argc = shell_parse_line(line, work, sizeof(work), argv);

    if (argc == 0) {
        return;
    }

    for (int i = 0; i < command_count; i++) {
        if (shell_str_equal(argv[0], commands[i].name)) {
            commands[i].fn(argc, argv);
            return;
        }
    }

    console_write("Unknown command: ");
    console_write(argv[0]);
    console_write("\n");
}