#pragma once

#define SHARED_BUFFER_KEY_MAX 64
#define SHARED_BUFFER_VALUE_MAX 1024
#define SHARED_BUFFER_CLIPBOARD_VALUE_MAX 16384
#define SHARED_BUFFER_MAX_ENTRIES 16

typedef enum SharedBufferStatus {
    SHARED_BUFFER_OK = 0,
    SHARED_BUFFER_ERR_INVALID_KEY,
    SHARED_BUFFER_ERR_VALUE_TOO_LONG,
    SHARED_BUFFER_ERR_FULL,
    SHARED_BUFFER_ERR_NOT_FOUND,
    SHARED_BUFFER_ERR_OUTPUT_TOO_SMALL
} SharedBufferStatus;

void shared_buffer_clear_all(void);

int shared_buffer_exists(const char* key);

SharedBufferStatus shared_buffer_set(
    const char* key,
    const char* value
);

SharedBufferStatus shared_buffer_get(
    const char* key,
    char* output,
    int output_size
);

SharedBufferStatus shared_buffer_take(
    const char* key,
    char* output,
    int output_size
);

SharedBufferStatus shared_buffer_clear(const char* key);

SharedBufferStatus shared_buffer_clipboard_set(const char* value);

SharedBufferStatus shared_buffer_clipboard_get(
    char* output,
    int output_size
);

SharedBufferStatus shared_buffer_clipboard_clear(void);

const char* shared_buffer_status_string(
    SharedBufferStatus status
);
