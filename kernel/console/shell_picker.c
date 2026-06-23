#include "shell_picker.h"

#include "shell_commands.h"
#include "shell_limits.h"
#include "shell_util.h"

static char shell_picker_result[SHELL_PICKER_RESULT_MAX];
static int shell_picker_has_result = 0;

void shell_picker_clear_result(void) {
    shell_picker_result[0] = '\0';
    shell_picker_has_result = 0;
}

int shell_picker_set_result(const char* path) {
    char resolved[SHELL_PATH_MAX];

    if (
        path == 0 ||
        path[0] == '\0' ||
        shell_resolve_path(path, resolved, sizeof(resolved)) != 0
    ) {
        return -1;
    }

    if (
        shell_text_length(resolved) >=
        (int)sizeof(shell_picker_result)
    ) {
        return -1;
    }

    shell_text_copy(
        shell_picker_result,
        sizeof(shell_picker_result),
        resolved
    );

    shell_picker_has_result = 1;
    return 0;
}

int shell_picker_take_result(
    char* output,
    int output_size
) {
    int length;

    if (output == 0 || output_size <= 0) {
        return -1;
    }

    output[0] = '\0';

    if (!shell_picker_has_result) {
        return 0;
    }

    length = shell_text_length(shell_picker_result);

    if (length + 1 > output_size) {
        return -1;
    }

    shell_text_copy(output, output_size, shell_picker_result);
    shell_picker_clear_result();
    return 1;
}
