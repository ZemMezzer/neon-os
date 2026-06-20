#include <stddef.h>
#include <stdint.h>

#include "errno.h"
#include "stdio.h"
#include "string.h"

#include "ff.h"
#include "console.h"

#define NEON_STDIO_MAX_OPEN_FILES 16
#define NEON_STDIO_PATH_MAX 512
#define NEON_STDIO_FORMAT_BUFFER 1024

#define NEON_FILE_FREE   0
#define NEON_FILE_FAT    1
#define NEON_FILE_STDIN  2
#define NEON_FILE_STDOUT 3
#define NEON_FILE_STDERR 4

struct FILE {
    int used;
    int eof;
    int error;

    int has_ungot;
    int ungot;

    int kind;
    int append;
    int remove_on_close;

    FIL* fil;
    char path[NEON_STDIO_PATH_MAX];
};

static FATFS stdio_fs;
static int stdio_mounted = 0;
static unsigned int stdio_tmp_counter = 0;

static FILE open_files[NEON_STDIO_MAX_OPEN_FILES];
static FIL open_fil_storage[NEON_STDIO_MAX_OPEN_FILES];

static FILE stdin_object = {
    .used = 1,
    .kind = NEON_FILE_STDIN,
    .fil = NULL
};

static FILE stdout_object = {
    .used = 1,
    .kind = NEON_FILE_STDOUT,
    .fil = NULL
};

static FILE stderr_object = {
    .used = 1,
    .kind = NEON_FILE_STDERR,
    .fil = NULL
};

FILE* stdin = &stdin_object;
FILE* stdout = &stdout_object;
FILE* stderr = &stderr_object;


static void stdio_set_errno_from_fresult(FRESULT result) {
    switch (result) {
        case FR_OK:
            errno = 0;
            break;
        case FR_NO_FILE:
        case FR_NO_PATH:
            errno = ENOENT;
            break;
        case FR_INVALID_NAME:
            errno = EINVAL;
            break;
        case FR_DENIED:
            errno = EACCES;
            break;
        case FR_EXIST:
            errno = EEXIST;
            break;
        case FR_WRITE_PROTECTED:
            errno = EROFS;
            break;
        case FR_NOT_ENOUGH_CORE:
            errno = ENOMEM;
            break;
        case FR_TOO_MANY_OPEN_FILES:
            errno = EMFILE;
            break;
        case FR_INVALID_OBJECT:
            errno = EBADF;
            break;
        case FR_INVALID_PARAMETER:
            errno = EINVAL;
            break;
        case FR_TIMEOUT:
            errno = EAGAIN;
            break;
        case FR_NOT_READY:
        case FR_NO_FILESYSTEM:
        case FR_DISK_ERR:
        case FR_INT_ERR:
        default:
            errno = EIO;
            break;
    }
}


static int stdio_is_console(FILE* stream) {
    if (stream == NULL) {
        return 0;
    }

    return stream->kind == NEON_FILE_STDOUT ||
           stream->kind == NEON_FILE_STDERR;
}


static int stdio_is_stdin(FILE* stream) {
    return stream != NULL && stream->kind == NEON_FILE_STDIN;
}


static void stdio_clear_path(FILE* stream) {
    if (stream != NULL) {
        stream->path[0] = '\0';
    }
}


static void stdio_copy_path(FILE* stream, const char* path) {
    size_t i = 0;

    if (stream == NULL) {
        return;
    }

    if (path == NULL) {
        stream->path[0] = '\0';
        return;
    }

    while (path[i] != '\0' && i + 1 < sizeof(stream->path)) {
        stream->path[i] = path[i];
        i++;
    }

    stream->path[i] = '\0';
}


static void stdio_reset_file_fields(FILE* stream, int index) {
    volatile FILE* vstream;

    if (stream == NULL) {
        return;
    }

    /*
        Keep every store visible. This is also consistent with the older
        stable stdio implementation in NeonOS.
    */
    vstream = (volatile FILE*)stream;

    vstream->used = 0;
    vstream->eof = 0;
    vstream->error = 0;
    vstream->has_ungot = 0;
    vstream->ungot = 0;
    vstream->kind = NEON_FILE_FREE;
    vstream->append = 0;
    vstream->remove_on_close = 0;
    vstream->path[0] = '\0';

    if (index >= 0 && index < NEON_STDIO_MAX_OPEN_FILES) {
        vstream->fil = &open_fil_storage[index];
    } else {
        vstream->fil = NULL;
    }
}


static BYTE stdio_mkfs_work[FF_MAX_SS];

static int stdio_format_and_mount(void) {
    MKFS_PARM options;
    FRESULT result;

    options.fmt = FM_FAT32 | FM_SFD;
    options.n_fat = 0;
    options.align = 0;
    options.n_root = 0;
    options.au_size = 0;

    (void)f_mount(NULL, "0:", 0);

    result = f_mkfs(
        "0:",
        &options,
        stdio_mkfs_work,
        sizeof(stdio_mkfs_work)
    );

    if (result != FR_OK) {
        stdio_set_errno_from_fresult(result);
        return -1;
    }

    result = f_mount(&stdio_fs, "0:", 1);

    if (result != FR_OK) {
        stdio_set_errno_from_fresult(result);
        return -1;
    }

    stdio_mounted = 1;
    return 0;
}

static int stdio_ensure_mounted(void) {
    FRESULT result;

    if (stdio_mounted) {
        return 0;
    }

    result = f_mount(&stdio_fs, "0:", 1);

    if (result == FR_NO_FILESYSTEM) {
        console_write("No filesystem, formatting disk...\n");
        return stdio_format_and_mount();
    }

    if (result != FR_OK) {
        stdio_set_errno_from_fresult(result);
        return -1;
    }

    stdio_mounted = 1;
    return 0;
}


int stdio_init(void) {
    int result;

    for (int i = 0; i < NEON_STDIO_MAX_OPEN_FILES; i++) {

        stdio_reset_file_fields(&open_files[i], i);
    }

    stdin_object.used = 1;
    stdin_object.eof = 0;
    stdin_object.error = 0;
    stdin_object.kind = NEON_FILE_STDIN;

    stdout_object.used = 1;
    stdout_object.eof = 0;
    stdout_object.error = 0;
    stdout_object.kind = NEON_FILE_STDOUT;

    stderr_object.used = 1;
    stderr_object.eof = 0;
    stderr_object.error = 0;
    stderr_object.kind = NEON_FILE_STDERR;

    stdio_mounted = 0;
    result = stdio_ensure_mounted();

    return result;
}


static int stdio_copy_char(char* out, size_t out_size, size_t* position, char ch) {
    if (*position + 1 >= out_size) {
        return -1;
    }

    if (ch == '\\') {
        ch = '/';
    }

    out[*position] = ch;
    *position = *position + 1;
    out[*position] = '\0';

    return 0;
}


static int stdio_make_fat_path(const char* path, char* out, size_t out_size) {
    size_t position = 0;
    size_t input_position = 0;

    if (path == NULL || out == NULL || out_size == 0 || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    out[0] = '\0';

    /*
        Lua's default package.path starts with "./?.lua".
        FatFs does not need that prefix, so normalize it here.
    */
    while (
        path[input_position] == '.' &&
        (path[input_position + 1] == '/' || path[input_position + 1] == '\\')
    ) {
        input_position += 2;
    }

    if (
        path[input_position] >= '0' &&
        path[input_position] <= '9' &&
        path[input_position + 1] == ':'
    ) {
        while (path[input_position] != '\0') {
            if (
                stdio_copy_char(
                    out,
                    out_size,
                    &position,
                    path[input_position]
                ) != 0
            ) {
                errno = ENAMETOOLONG;
                return -1;
            }

            input_position++;
        }

        return 0;
    }

    if (stdio_copy_char(out, out_size, &position, '0') != 0 ||
        stdio_copy_char(out, out_size, &position, ':') != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (path[input_position] != '/' && path[input_position] != '\\') {
        if (stdio_copy_char(out, out_size, &position, '/') != 0) {
            errno = ENAMETOOLONG;
            return -1;
        }
    }

    while (path[input_position] != '\0') {
        if (
            stdio_copy_char(
                out,
                out_size,
                &position,
                path[input_position]
            ) != 0
        ) {
            errno = ENAMETOOLONG;
            return -1;
        }

        input_position++;
    }

    return 0;
}


static FILE* stdio_alloc_file(void) {
    for (int i = 0; i < NEON_STDIO_MAX_OPEN_FILES; i++) {
        if (!open_files[i].used) {
            open_files[i].used = 1;
            open_files[i].eof = 0;
            open_files[i].error = 0;
            open_files[i].has_ungot = 0;
            open_files[i].ungot = 0;
            open_files[i].kind = NEON_FILE_FAT;
            open_files[i].append = 0;
            open_files[i].remove_on_close = 0;
            open_files[i].fil = &open_fil_storage[i];
            stdio_clear_path(&open_files[i]);

            return &open_files[i];
        }
    }

    errno = EMFILE;
    return NULL;
}


static void stdio_free_file(FILE* stream) {
    int index = -1;

    if (stream == NULL) {
        return;
    }

    for (int i = 0; i < NEON_STDIO_MAX_OPEN_FILES; i++) {
        if (stream == &open_files[i]) {
            index = i;
            break;
        }
    }

    stdio_reset_file_fields(stream, index);
}


static int stdio_parse_mode(const char* mode, BYTE* out_mode, int* out_append) {
    int read = 0;
    int write = 0;
    int create = 0;
    int truncate = 0;
    int append = 0;
    int plus_seen = 0;

    if (mode == NULL || out_mode == NULL || out_append == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (mode[0] == 'r') {
        read = 1;
    } else if (mode[0] == 'w') {
        write = 1;
        create = 1;
        truncate = 1;
    } else if (mode[0] == 'a') {
        write = 1;
        create = 1;
        append = 1;
    } else {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 1; mode[i] != '\0'; i++) {
        if (mode[i] == '+') {
            if (plus_seen) {
                errno = EINVAL;
                return -1;
            }

            plus_seen = 1;
            read = 1;
            write = 1;
        } else if (mode[i] != 'b' && mode[i] != 't') {
            errno = EINVAL;
            return -1;
        }
    }

    *out_mode = 0;

    if (read) {
        *out_mode |= FA_READ;
    }

    if (write) {
        *out_mode |= FA_WRITE;
    }

    if (truncate) {
        *out_mode |= FA_CREATE_ALWAYS;
    } else if (create && append) {
        *out_mode |= FA_OPEN_ALWAYS;
    }

    *out_append = append;
    return 0;
}


static FILE* stdio_open_into(FILE* stream, const char* path, const char* mode) {
    char fat_path[NEON_STDIO_PATH_MAX];
    BYTE fat_mode;
    int append;
    FRESULT result;

    if (stream == NULL || stream->fil == NULL) {
        errno = EBADF;
        return NULL;
    }

    if (stdio_ensure_mounted() != 0) {
        return NULL;
    }

    if (stdio_make_fat_path(path, fat_path, sizeof(fat_path)) != 0) {
        return NULL;
    }

    if (stdio_parse_mode(mode, &fat_mode, &append) != 0) {
        return NULL;
    }

    result = f_open(stream->fil, fat_path, fat_mode);

    if (result != FR_OK) {
        stdio_set_errno_from_fresult(result);
        return NULL;
    }

    stream->used = 1;
    stream->eof = 0;
    stream->error = 0;
    stream->has_ungot = 0;
    stream->ungot = 0;
    stream->kind = NEON_FILE_FAT;
    stream->append = append;
    stream->remove_on_close = 0;
    stdio_copy_path(stream, fat_path);

    if (append) {
        result = f_lseek(stream->fil, f_size(stream->fil));

        if (result != FR_OK) {
            (void)f_close(stream->fil);
            stdio_set_errno_from_fresult(result);
            return NULL;
        }
    }

    return stream;
}


FILE* fopen(const char* path, const char* mode) {
    FILE* stream = stdio_alloc_file();

    if (stream == NULL) {
        return NULL;
    }

    if (stdio_open_into(stream, path, mode) == NULL) {
        stdio_free_file(stream);
        return NULL;
    }

    return stream;
}


FILE* freopen(const char* path, const char* mode, FILE* stream) {
    FRESULT result;

    if (
        stream == NULL ||
        !stream->used ||
        stdio_is_console(stream) ||
        stdio_is_stdin(stream) ||
        stream->kind != NEON_FILE_FAT ||
        stream->fil == NULL
    ) {
        errno = EBADF;
        return NULL;
    }

    result = f_close(stream->fil);

    if (result != FR_OK) {
        stream->error = 1;
        stdio_set_errno_from_fresult(result);
        return NULL;
    }

    stream->eof = 0;
    stream->error = 0;
    stream->has_ungot = 0;
    stream->ungot = 0;
    stream->append = 0;
    stream->remove_on_close = 0;
    stdio_clear_path(stream);

    return stdio_open_into(stream, path, mode);
}


static void stdio_append_unsigned(char* out, size_t out_size, size_t* position, unsigned int value) {
    char digits[10];
    size_t count = 0;

    if (value == 0) {
        if (*position + 1 < out_size) {
            out[*position] = '0';
            *position = *position + 1;
            out[*position] = '\0';
        }

        return;
    }

    while (value != 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (count != 0) {
        count--;

        if (*position + 1 >= out_size) {
            break;
        }

        out[*position] = digits[count];
        *position = *position + 1;
        out[*position] = '\0';
    }
}


char* tmpnam(char* buffer) {
    static char fallback[L_tmpnam];
    size_t position = 0;

    if (buffer == NULL) {
        buffer = fallback;
    }

    buffer[0] = '\0';

    const char* prefix = "0:/.__lua_tmp_";

    while (*prefix != '\0' && position + 1 < L_tmpnam) {
        buffer[position++] = *prefix++;
    }

    stdio_tmp_counter++;
    stdio_append_unsigned(buffer, L_tmpnam, &position, stdio_tmp_counter);

    const char* suffix = ".tmp";

    while (*suffix != '\0' && position + 1 < L_tmpnam) {
        buffer[position++] = *suffix++;
    }

    buffer[position] = '\0';
    return buffer;
}


FILE* tmpfile(void) {
    char path[L_tmpnam];
    FILE* stream;

    (void)tmpnam(path);

    stream = fopen(path, "w+");

    if (stream == NULL) {
        return NULL;
    }

    stream->remove_on_close = 1;
    return stream;
}


int fclose(FILE* stream) {
    FRESULT result;
    int remove_after_close;
    char path[NEON_STDIO_PATH_MAX];

    if (stream == NULL || !stream->used) {
        errno = EBADF;
        return EOF;
    }

    if (stdio_is_console(stream) || stdio_is_stdin(stream)) {
        return 0;
    }

    if (stream->fil == NULL) {
        errno = EBADF;
        return EOF;
    }

    remove_after_close = stream->remove_on_close;
    for (size_t i = 0; i < sizeof(path); i++) {
        path[i] = stream->path[i];
        if (path[i] == '\0') {
            break;
        }
    }

    result = f_close(stream->fil);

    if (result != FR_OK) {
        stream->error = 1;
        stdio_set_errno_from_fresult(result);
        stdio_free_file(stream);
        return EOF;
    }

    stdio_free_file(stream);

    if (remove_after_close && path[0] != '\0') {
        result = f_unlink(path);

        if (result != FR_OK) {
            stdio_set_errno_from_fresult(result);
            return EOF;
        }
    }

    return 0;
}


size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
    size_t total;
    size_t done = 0;
    UINT bytes_read = 0;
    FRESULT result;
    uint8_t* output = (uint8_t*)ptr;

    if (ptr == NULL || stream == NULL || !stream->used || size == 0 || count == 0) {
        return 0;
    }

    if (size > (size_t)-1 / count) {
        stream->error = 1;
        errno = EINVAL;
        return 0;
    }

    if (stdio_is_stdin(stream)) {
        /*
            Keyboard-backed stdin is not wired to the C FILE layer yet.
            Lua io.read() will return EOF instead of blocking the kernel.
        */
        stream->eof = 1;
        return 0;
    }

    if (stdio_is_console(stream) || stream->fil == NULL) {
        stream->error = 1;
        errno = EBADF;
        return 0;
    }

    total = size * count;

    if (stream->has_ungot && total > 0) {
        output[0] = (uint8_t)stream->ungot;
        stream->has_ungot = 0;
        done = 1;
    }

    if (done < total) {
        result = f_read(
            stream->fil,
            output + done,
            (UINT)(total - done),
            &bytes_read
        );

        if (result != FR_OK) {
            stream->error = 1;
            stdio_set_errno_from_fresult(result);
            return done / size;
        }

        done += bytes_read;
    }

    if (done < total) {
        stream->eof = 1;
    }

    return done / size;
}


static void stdio_write_console_chunk(const char* data, size_t size) {
    char buffer[129];
    size_t position = 0;

    while (position < size) {
        size_t chunk = size - position;

        if (chunk > sizeof(buffer) - 1) {
            chunk = sizeof(buffer) - 1;
        }

        for (size_t i = 0; i < chunk; i++) {
            buffer[i] = data[position + i];
        }

        buffer[chunk] = '\0';

        console_write(buffer);

        position += chunk;
    }
}


size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
    size_t total;
    UINT bytes_written = 0;
    FRESULT result;

    if (ptr == NULL || stream == NULL || !stream->used || size == 0 || count == 0) {
        return 0;
    }

    if (size > (size_t)-1 / count) {
        stream->error = 1;
        errno = EINVAL;
        return 0;
    }

    total = size * count;

    if (stdio_is_console(stream)) {
        stdio_write_console_chunk((const char*)ptr, total);
        return count;
    }

    if (stdio_is_stdin(stream) || stream->fil == NULL) {
        stream->error = 1;
        errno = EBADF;
        return 0;
    }

    if (stream->append) {
        result = f_lseek(stream->fil, f_size(stream->fil));

        if (result != FR_OK) {
            stream->error = 1;
            stdio_set_errno_from_fresult(result);
            return 0;
        }
    }

    result = f_write(stream->fil, ptr, (UINT)total, &bytes_written);

    if (result != FR_OK) {
        stream->error = 1;
        stdio_set_errno_from_fresult(result);
        return bytes_written / size;
    }

    if (bytes_written < total) {
        stream->error = 1;
        errno = EIO;
    }

    return bytes_written / size;
}


int fflush(FILE* stream) {
    FRESULT result;
    int failed = 0;

    if (stream == NULL) {
        for (int i = 0; i < NEON_STDIO_MAX_OPEN_FILES; i++) {
            if (open_files[i].used && open_files[i].kind == NEON_FILE_FAT) {
                if (fflush(&open_files[i]) == EOF) {
                    failed = 1;
                }
            }
        }

        return failed ? EOF : 0;
    }

    if (!stream->used) {
        errno = EBADF;
        return EOF;
    }

    if (stdio_is_console(stream) || stdio_is_stdin(stream)) {
        return 0;
    }

    if (stream->fil == NULL) {
        errno = EBADF;
        return EOF;
    }

    result = f_sync(stream->fil);

    if (result != FR_OK) {
        stream->error = 1;
        stdio_set_errno_from_fresult(result);
        return EOF;
    }

    return 0;
}


int fseek(FILE* stream, long offset, int whence) {
    FSIZE_t base;
    FSIZE_t target;
    FRESULT result;

    if (
        stream == NULL ||
        !stream->used ||
        stdio_is_console(stream) ||
        stdio_is_stdin(stream) ||
        stream->fil == NULL
    ) {
        errno = EBADF;
        return -1;
    }

    if (whence == SEEK_SET) {
        if (offset < 0) {
            errno = EINVAL;
            return -1;
        }

        target = (FSIZE_t)offset;
    } else if (whence == SEEK_CUR) {
        base = f_tell(stream->fil);

        if (offset < 0 && (FSIZE_t)(-offset) > base) {
            errno = EINVAL;
            return -1;
        }

        target = (FSIZE_t)((long)base + offset);
    } else if (whence == SEEK_END) {
        base = f_size(stream->fil);

        if (offset < 0 && (FSIZE_t)(-offset) > base) {
            errno = EINVAL;
            return -1;
        }

        target = (FSIZE_t)((long)base + offset);
    } else {
        errno = EINVAL;
        return -1;
    }

    result = f_lseek(stream->fil, target);

    if (result != FR_OK) {
        stream->error = 1;
        stdio_set_errno_from_fresult(result);
        return -1;
    }

    stream->eof = 0;
    stream->has_ungot = 0;
    return 0;
}


long ftell(FILE* stream) {
    if (
        stream == NULL ||
        !stream->used ||
        stdio_is_console(stream) ||
        stdio_is_stdin(stream) ||
        stream->fil == NULL
    ) {
        errno = EBADF;
        return -1;
    }

    return (long)f_tell(stream->fil);
}


int feof(FILE* stream) {
    if (stream == NULL || !stream->used) {
        return 0;
    }

    return stream->eof;
}


int ferror(FILE* stream) {
    if (stream == NULL || !stream->used) {
        return 1;
    }

    return stream->error;
}


void clearerr(FILE* stream) {
    if (stream == NULL || !stream->used) {
        return;
    }

    stream->eof = 0;
    stream->error = 0;
}


int fgetc(FILE* stream) {
    uint8_t character;
    size_t read_count;

    if (stream == NULL || !stream->used) {
        errno = EBADF;
        return EOF;
    }

    if (stream->has_ungot) {
        stream->has_ungot = 0;
        return stream->ungot;
    }

    read_count = fread(&character, 1, 1, stream);

    if (read_count != 1) {
        return EOF;
    }

    return (int)character;
}


int getc(FILE* stream) {
    return fgetc(stream);
}


int getchar(void) {
    return fgetc(stdin);
}


int ungetc(int character, FILE* stream) {
    if (stream == NULL || !stream->used || character == EOF) {
        errno = EBADF;
        return EOF;
    }

    if (stream->has_ungot) {
        return EOF;
    }

    stream->ungot = character & 0xFF;
    stream->has_ungot = 1;
    stream->eof = 0;

    return stream->ungot;
}


char* fgets(char* string, int size, FILE* stream) {
    int character;
    int position = 0;

    if (string == NULL || size <= 0 || stream == NULL) {
        errno = EINVAL;
        return NULL;
    }

    while (position + 1 < size) {
        character = fgetc(stream);

        if (character == EOF) {
            break;
        }

        string[position++] = (char)character;

        if (character == '\n') {
            break;
        }
    }

    if (position == 0) {
        return NULL;
    }

    string[position] = '\0';
    return string;
}


int fputc(int character, FILE* stream) {
    uint8_t byte = (uint8_t)character;

    if (fwrite(&byte, 1, 1, stream) != 1) {
        return EOF;
    }

    return byte;
}


int putc(int character, FILE* stream) {
    return fputc(character, stream);
}


int putchar(int character) {
    return fputc(character, stdout);
}


int fputs(const char* string, FILE* stream) {
    size_t length;

    if (string == NULL || stream == NULL) {
        errno = EINVAL;
        return EOF;
    }

    length = strlen(string);

    if (fwrite(string, 1, length, stream) != length) {
        return EOF;
    }

    return 0;
}


int puts(const char* string) {
    if (fputs(string, stdout) == EOF) {
        return EOF;
    }

    return fputc('\n', stdout) == EOF ? EOF : 0;
}


int neon_vfprintf_ptr(FILE* stream, const char* format, va_list* args) {
    char buffer[NEON_STDIO_FORMAT_BUFFER];
    int formatted;
    size_t length;

    if (stream == NULL || format == NULL || args == NULL) {
        errno = EINVAL;
        return -1;
    }

    formatted = neon_vsnprintf_ptr(
        buffer,
        sizeof(buffer),
        format,
        args
    );

    if (formatted < 0) {
        stream->error = 1;
        errno = EIO;
        return -1;
    }

    length = strlen(buffer);

    if (fwrite(buffer, 1, length, stream) != length) {
        return -1;
    }

    return formatted;
}


int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    int result;

    va_start(args, format);
    result = neon_vfprintf_ptr(stream, format, &args);
    va_end(args);

    return result;
}


int printf(const char* format, ...) {
    va_list args;
    int result;

    va_start(args, format);
    result = neon_vfprintf_ptr(stdout, format, &args);
    va_end(args);

    return result;
}


int remove(const char* path) {
    char fat_path[NEON_STDIO_PATH_MAX];
    FRESULT result;

    if (stdio_ensure_mounted() != 0) {
        return -1;
    }

    if (stdio_make_fat_path(path, fat_path, sizeof(fat_path)) != 0) {
        return -1;
    }

    result = f_unlink(fat_path);

    if (result != FR_OK) {
        stdio_set_errno_from_fresult(result);
        return -1;
    }

    return 0;
}


int rename(const char* old_path, const char* new_path) {
    char old_fat_path[NEON_STDIO_PATH_MAX];
    char new_fat_path[NEON_STDIO_PATH_MAX];
    FRESULT result;

    if (stdio_ensure_mounted() != 0) {
        return -1;
    }

    if (stdio_make_fat_path(old_path, old_fat_path, sizeof(old_fat_path)) != 0 ||
        stdio_make_fat_path(new_path, new_fat_path, sizeof(new_fat_path)) != 0) {
        return -1;
    }

    result = f_rename(old_fat_path, new_fat_path);

    if (result != FR_OK) {
        stdio_set_errno_from_fresult(result);
        return -1;
    }

    return 0;
}


int setvbuf(FILE* stream, char* buffer, int mode, size_t size) {
    (void)buffer;
    (void)size;

    if (stream == NULL || !stream->used) {
        errno = EBADF;
        return -1;
    }

    if (mode != _IONBF && mode != _IOLBF && mode != _IOFBF) {
        errno = EINVAL;
        return -1;
    }

    /*
        FatFs is already the backing store; NeonOS keeps this layer
        unbuffered. Lua only needs the API contract and a success/failure.
    */
    return 0;
}


void perror(const char* string) {
    if (string != NULL && string[0] != '\0') {
        fputs(string, stderr);
        fputs(": ", stderr);
    }

    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}
