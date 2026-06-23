#include "shell_commands.h"

#include "shell_picker.h"
#include "shell_util.h"

#include "program_runtime.h"

#include "console.h"

#if SHELL_STD
#include "register.h"
#endif

#if PACKAGES_ENABLED
#include "package_shell.h"
#endif

typedef struct ShellCommand {
    const char* name;
    const char* help;
    ShellCommandFn fn;
} ShellCommand;

static ShellCommand commands[SHELL_MAX_COMMANDS];
static int command_count = 0;
static ShellCommandFallbackFn command_fallback = 0;

static int shell_parse_line(
    const char* line,
    char* work,
    int work_size,
    char** argv
) {
    int argc = 0;
    int write_index = 0;
    int in_quote = 0;
    int token_started = 0;
    int line_index;

    if (line == 0 || work == 0 || argv == 0 || work_size <= 0) {
        return 0;
    }

    for (line_index = 0; line[line_index] != '\0'; line_index++) {
        char character = line[line_index];

        if (character == '"') {
            if (!token_started) {
                if (argc >= SHELL_MAX_ARGS) {
                    break;
                }

                argv[argc++] = &work[write_index];
                token_started = 1;
            }

            in_quote = !in_quote;
            continue;
        }

        if (
            !in_quote &&
            (character == ' ' || character == '\t')
        ) {
            if (token_started) {
                if (write_index + 1 >= work_size) {
                    break;
                }

                work[write_index++] = '\0';
                token_started = 0;
            }

            continue;
        }

        if (!token_started) {
            if (argc >= SHELL_MAX_ARGS) {
                break;
            }

            argv[argc++] = &work[write_index];
            token_started = 1;
        }

        if (write_index + 1 >= work_size) {
            break;
        }

        work[write_index++] = character;
    }

    if (token_started && write_index < work_size) {
        work[write_index++] = '\0';
    }

    if (write_index < work_size) {
        work[write_index] = '\0';
    }

    return argc;
}

int shell_register_command(
    const char* name,
    const char* help,
    ShellCommandFn fn
) {
    if (name == 0 || help == 0 || fn == 0) {
        return -1;
    }

    if (command_count >= SHELL_MAX_COMMANDS) {
        return -1;
    }

    commands[command_count].name = name;
    commands[command_count].help = help;
    commands[command_count].fn = fn;
    command_count++;

    return 0;
}

int shell_set_command_fallback(ShellCommandFallbackFn fn) {
    command_fallback = fn;
    return 0;
}

int shell_visit_commands(
    ShellCommandVisitor visitor,
    void* user_data
) {
    int index;

    if (visitor == 0) {
        return -1;
    }

    for (index = 0; index < command_count; index++) {
        int result = visitor(
            commands[index].name,
            commands[index].help,
            user_data
        );

        if (result != 0) {
            return result;
        }
    }

    return 0;
}

int shell_commands_execute(const char* line) {
    char work[SHELL_LINE_MAX];
    char* argv[SHELL_MAX_ARGS];
    int argc;
    int index;

    if (line == 0 || line[0] == '\0') {
        return 0;
    }

    argc = shell_parse_line(line, work, sizeof(work), argv);

    if (argc == 0) {
        return 0;
    }

    for (index = 0; index < command_count; index++) {
        if (shell_text_equal(argv[0], commands[index].name)) {
            return commands[index].fn(argc, argv);
        }
    }

    if (command_fallback != 0) {
        int fallback_status = 0;

        if (command_fallback(argc, argv, &fallback_status) != 0) {
            return fallback_status;
        }
    }

    console_write("Unknown command: ");
    console_write(argv[0]);
    console_write("\n");

    return 127;
}

void shell_commands_init(void) {
    program_set_command_executor(shell_commands_execute);
    shell_picker_clear_result();

#if SHELL_STD
    shell_bin_register_commands();
#endif

#if PACKAGES_ENABLED
    package_shell_register_commands();
#endif
}
