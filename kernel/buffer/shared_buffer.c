#include "shared_buffer.h"

#include <stddef.h>

typedef struct SharedBufferEntry {
    int used;
    char key[SHARED_BUFFER_KEY_MAX];
    char value[SHARED_BUFFER_VALUE_MAX];
} SharedBufferEntry;

static SharedBufferEntry shared_buffer_entries[SHARED_BUFFER_MAX_ENTRIES];
static char shared_buffer_clipboard[SHARED_BUFFER_CLIPBOARD_VALUE_MAX];
static int shared_buffer_clipboard_used = 0;

static int shared_buffer_length(const char* text) {
    int length = 0;

    if (text == NULL) {
        return -1;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

static int shared_buffer_key_is_valid(const char* key) {
    int index = 0;

    if (key == NULL || key[0] == '\0') {
        return 0;
    }

    while (key[index] != '\0') {
        unsigned char character = (unsigned char)key[index];

        if (character < 32U || character == 127U) {
            return 0;
        }

        index++;

        if (index >= SHARED_BUFFER_KEY_MAX) {
            return 0;
        }
    }

    return 1;
}

static int shared_buffer_equal(
    const char* left,
    const char* right
) {
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

static void shared_buffer_copy(
    char* output,
    int output_size,
    const char* input
) {
    int index = 0;

    if (output == NULL || output_size <= 0) {
        return;
    }

    if (input == NULL) {
        output[0] = '\0';
        return;
    }

    while (input[index] != '\0' && index + 1 < output_size) {
        output[index] = input[index];
        index++;
    }

    output[index] = '\0';
}

static void shared_buffer_entry_clear(SharedBufferEntry* entry) {
    if (entry == NULL) {
        return;
    }

    entry->used = 0;
    entry->key[0] = '\0';
    entry->value[0] = '\0';
}

static int shared_buffer_find(const char* key) {
    int index;

    for (index = 0; index < SHARED_BUFFER_MAX_ENTRIES; index++) {
        if (
            shared_buffer_entries[index].used &&
            shared_buffer_equal(shared_buffer_entries[index].key, key)
        ) {
            return index;
        }
    }

    return -1;
}

static int shared_buffer_find_free(void) {
    int index;

    for (index = 0; index < SHARED_BUFFER_MAX_ENTRIES; index++) {
        if (!shared_buffer_entries[index].used) {
            return index;
        }
    }

    return -1;
}

void shared_buffer_clear_all(void) {
    int index;

    for (index = 0; index < SHARED_BUFFER_MAX_ENTRIES; index++) {
        shared_buffer_entry_clear(&shared_buffer_entries[index]);
    }

    shared_buffer_clipboard[0] = '\0';
    shared_buffer_clipboard_used = 0;
}

int shared_buffer_exists(const char* key) {
    if (!shared_buffer_key_is_valid(key)) {
        return 0;
    }

    return shared_buffer_find(key) >= 0;
}

SharedBufferStatus shared_buffer_set(
    const char* key,
    const char* value
) {
    int value_length;
    int index;

    if (!shared_buffer_key_is_valid(key)) {
        return SHARED_BUFFER_ERR_INVALID_KEY;
    }

    value_length = shared_buffer_length(value);

    if (value_length < 0 || value_length >= SHARED_BUFFER_VALUE_MAX) {
        return SHARED_BUFFER_ERR_VALUE_TOO_LONG;
    }

    index = shared_buffer_find(key);

    if (index < 0) {
        index = shared_buffer_find_free();

        if (index < 0) {
            return SHARED_BUFFER_ERR_FULL;
        }

        shared_buffer_entries[index].used = 1;
        shared_buffer_copy(
            shared_buffer_entries[index].key,
            sizeof(shared_buffer_entries[index].key),
            key
        );
    }

    shared_buffer_copy(
        shared_buffer_entries[index].value,
        sizeof(shared_buffer_entries[index].value),
        value
    );

    return SHARED_BUFFER_OK;
}

SharedBufferStatus shared_buffer_get(
    const char* key,
    char* output,
    int output_size
) {
    int index;
    int value_length;

    if (!shared_buffer_key_is_valid(key)) {
        return SHARED_BUFFER_ERR_INVALID_KEY;
    }

    if (output == NULL || output_size <= 0) {
        return SHARED_BUFFER_ERR_OUTPUT_TOO_SMALL;
    }

    index = shared_buffer_find(key);

    if (index < 0) {
        output[0] = '\0';
        return SHARED_BUFFER_ERR_NOT_FOUND;
    }

    value_length = shared_buffer_length(shared_buffer_entries[index].value);

    if (value_length + 1 > output_size) {
        output[0] = '\0';
        return SHARED_BUFFER_ERR_OUTPUT_TOO_SMALL;
    }

    shared_buffer_copy(
        output,
        output_size,
        shared_buffer_entries[index].value
    );

    return SHARED_BUFFER_OK;
}

SharedBufferStatus shared_buffer_take(
    const char* key,
    char* output,
    int output_size
) {
    int index;
    SharedBufferStatus status;

    status = shared_buffer_get(key, output, output_size);

    if (status != SHARED_BUFFER_OK) {
        return status;
    }

    index = shared_buffer_find(key);

    if (index >= 0) {
        shared_buffer_entry_clear(&shared_buffer_entries[index]);
    }

    return SHARED_BUFFER_OK;
}

SharedBufferStatus shared_buffer_clear(const char* key) {
    int index;

    if (!shared_buffer_key_is_valid(key)) {
        return SHARED_BUFFER_ERR_INVALID_KEY;
    }

    index = shared_buffer_find(key);

    if (index < 0) {
        return SHARED_BUFFER_ERR_NOT_FOUND;
    }

    shared_buffer_entry_clear(&shared_buffer_entries[index]);
    return SHARED_BUFFER_OK;
}


SharedBufferStatus shared_buffer_clipboard_set(const char* value) {
    int value_length = shared_buffer_length(value);

    if (
        value_length < 0 ||
        value_length >= SHARED_BUFFER_CLIPBOARD_VALUE_MAX
    ) {
        return SHARED_BUFFER_ERR_VALUE_TOO_LONG;
    }

    shared_buffer_copy(
        shared_buffer_clipboard,
        sizeof(shared_buffer_clipboard),
        value
    );
    shared_buffer_clipboard_used = 1;
    return SHARED_BUFFER_OK;
}

SharedBufferStatus shared_buffer_clipboard_get(
    char* output,
    int output_size
) {
    int value_length;

    if (output == NULL || output_size <= 0) {
        return SHARED_BUFFER_ERR_OUTPUT_TOO_SMALL;
    }

    if (!shared_buffer_clipboard_used) {
        output[0] = '\0';
        return SHARED_BUFFER_ERR_NOT_FOUND;
    }

    value_length = shared_buffer_length(shared_buffer_clipboard);

    if (value_length + 1 > output_size) {
        output[0] = '\0';
        return SHARED_BUFFER_ERR_OUTPUT_TOO_SMALL;
    }

    shared_buffer_copy(
        output,
        output_size,
        shared_buffer_clipboard
    );

    return SHARED_BUFFER_OK;
}

SharedBufferStatus shared_buffer_clipboard_clear(void) {
    int existed = shared_buffer_clipboard_used;

    shared_buffer_clipboard[0] = '\0';
    shared_buffer_clipboard_used = 0;

    return existed
        ? SHARED_BUFFER_OK
        : SHARED_BUFFER_ERR_NOT_FOUND;
}

const char* shared_buffer_status_string(
    SharedBufferStatus status
) {
    switch (status) {
        case SHARED_BUFFER_OK:
            return "ok";
        case SHARED_BUFFER_ERR_INVALID_KEY:
            return "invalid buffer key";
        case SHARED_BUFFER_ERR_VALUE_TOO_LONG:
            return "buffer value is too long";
        case SHARED_BUFFER_ERR_FULL:
            return "buffer is full";
        case SHARED_BUFFER_ERR_NOT_FOUND:
            return "buffer value not found";
        case SHARED_BUFFER_ERR_OUTPUT_TOO_SMALL:
            return "buffer output is too small";
        default:
            return "unknown buffer error";
    }
}
