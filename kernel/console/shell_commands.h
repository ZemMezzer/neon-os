typedef int (*ShellCommandFn)(int argc, char** argv);

typedef int (*ShellCommandFallbackFn)(
    int argc,
    char** argv,
    int* out_status
);

int shell_register_command(
    const char* name,
    const char* help,
    ShellCommandFn fn
);

int shell_set_command_fallback(ShellCommandFallbackFn fn);

int shell_resolve_path(
    const char* input,
    char* output,
    int output_size
);

int shell_get_current_directory(
    char* output,
    int output_size
);

int shell_set_current_directory(const char* input);

int shell_find_command_file(
    const char* filename,
    char* output,
    int output_size
);

int shell_run_script(const char* input_path);

int shell_commands_execute(const char* line);

void shell_commands_init(void);