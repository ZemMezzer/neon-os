#include <stddef.h>
#include <stdint.h>

#include "stdio.h"
#include "string.h"

#include "ff.h"
#include "console.h"
#include "uart.h"

#define NEON_STDIO_MAX_OPEN_FILES 16
#define NEON_STDIO_PATH_MAX 512

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

    FIL* fil;
};

static FATFS stdio_fs;
static int stdio_mounted = 0;

static FILE open_files[NEON_STDIO_MAX_OPEN_FILES];
static FIL open_fil_storage[NEON_STDIO_MAX_OPEN_FILES];

static FILE stdin_object = {
    .used = 1,
    .kind = NEON_FILE_STDIN,
    .fil = 0
};

static FILE stdout_object = {
    .used = 1,
    .kind = NEON_FILE_STDOUT,
    .fil = 0
};

static FILE stderr_object = {
    .used = 1,
    .kind = NEON_FILE_STDERR,
    .fil = 0
};

FILE* stdin = &stdin_object;
FILE* stdout = &stdout_object;
FILE* stderr = &stderr_object;

static int stdio_is_console(FILE* stream) {
    if (!stream) {
        return 0;
    }

    return stream->kind == NEON_FILE_STDOUT || stream->kind == NEON_FILE_STDERR;
}

static int stdio_is_stdin(FILE* stream) {
    if (!stream) {
        return 0;
    }

    return stream->kind == NEON_FILE_STDIN;
}

static void stdio_reset_file_fields(FILE* stream, int index) {
    volatile FILE* vstream;

    if (!stream) {
        return;
    }

    vstream = (volatile FILE*)stream;

    vstream->used = 0;
    vstream->eof = 0;
    vstream->error = 0;
    vstream->has_ungot = 0;
    vstream->ungot = 0;
    vstream->kind = NEON_FILE_FREE;
    vstream->append = 0;

    if (index >= 0 && index < NEON_STDIO_MAX_OPEN_FILES) {
        vstream->fil = &open_fil_storage[index];
    } else {
        vstream->fil = 0;
    }
}

static int stdio_ensure_mounted(void) {
    FRESULT res;

    if (stdio_mounted) {
        return 0;
    }

    res = f_mount(&stdio_fs, "0:", 1);

    if (res != FR_OK) {
        return -1;
    }

    stdio_mounted = 1;

    return 0;
}

int stdio_init(void) {
    volatile int* mounted_ptr = (volatile int*)&stdio_mounted;

    for (int i = 0; i < NEON_STDIO_MAX_OPEN_FILES; i++) {
        stdio_reset_file_fields(&open_files[i], i);
    }

    *mounted_ptr = 0;

    return stdio_ensure_mounted();
}

static int stdio_copy_char(char* out, size_t out_size, size_t* pos, char ch) {
    if (*pos + 1 >= out_size) {
        return -1;
    }

    if (ch == '\\') {
        ch = '/';
    }

    out[*pos] = ch;
    *pos = *pos + 1;
    out[*pos] = 0;

    return 0;
}

static int stdio_make_fat_path(const char* path, char* out, size_t out_size) {
    size_t pos = 0;
    size_t i = 0;

    if (!path || !out || out_size == 0) {
        return -1;
    }

    out[0] = 0;

    if (path[0] == 0) {
        return -1;
    }

    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':') {
        while (path[i] != 0) {
            if (stdio_copy_char(out, out_size, &pos, path[i]) != 0) {
                return -1;
            }

            i++;
        }

        return 0;
    }

    if (stdio_copy_char(out, out_size, &pos, '0') != 0) {
        return -1;
    }

    if (stdio_copy_char(out, out_size, &pos, ':') != 0) {
        return -1;
    }

    if (path[0] != '/' && path[0] != '\\') {
        if (stdio_copy_char(out, out_size, &pos, '/') != 0) {
            return -1;
        }
    }

    while (path[i] != 0) {
        if (stdio_copy_char(out, out_size, &pos, path[i]) != 0) {
            return -1;
        }

        i++;
    }

    return 0;
}

static FILE* stdio_alloc_file(void) {
    for (int i = 0; i < NEON_STDIO_MAX_OPEN_FILES; i++) {
        volatile FILE* vf = (volatile FILE*)&open_files[i];

        if (!vf->used) {
            vf->used = 1;
            vf->eof = 0;
            vf->error = 0;
            vf->has_ungot = 0;
            vf->ungot = 0;
            vf->kind = NEON_FILE_FAT;
            vf->append = 0;
            vf->fil = &open_fil_storage[i];

            return &open_files[i];
        }
    }

    return NULL;
}

static void stdio_free_file(FILE* stream) {
    volatile FILE* vf;

    if (!stream) {
        return;
    }

    vf = (volatile FILE*)stream;

    vf->used = 0;
    vf->eof = 0;
    vf->error = 0;
    vf->has_ungot = 0;
    vf->ungot = 0;
    vf->kind = NEON_FILE_FREE;
    vf->append = 0;
}

static int stdio_parse_mode(const char* mode, BYTE* out_mode, int* out_append) {
    int read = 0;
    int write = 0;
    int create = 0;
    int truncate = 0;
    int append = 0;

    if (!mode || !out_mode || !out_append) {
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
        return -1;
    }

    for (size_t i = 1; mode[i] != 0; i++) {
        if (mode[i] == '+') {
            read = 1;
            write = 1;
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

FILE* fopen(const char* path, const char* mode) {
    char fat_path[NEON_STDIO_PATH_MAX];
    BYTE fat_mode;
    int append;
    FRESULT res;
    FILE* stream;

    if (stdio_ensure_mounted() != 0) {
        return NULL;
    }

    if (stdio_make_fat_path(path, fat_path, sizeof(fat_path)) != 0) {
        return NULL;
    }

    if (stdio_parse_mode(mode, &fat_mode, &append) != 0) {
        return NULL;
    }

    stream = stdio_alloc_file();

    if (!stream || !stream->fil) {
        return NULL;
    }

    res = f_open(stream->fil, fat_path, fat_mode);

    if (res != FR_OK) {
        stdio_free_file(stream);
        return NULL;
    }

    stream->append = append;

    if (append) {
        res = f_lseek(stream->fil, f_size(stream->fil));

        if (res != FR_OK) {
            f_close(stream->fil);
            stdio_free_file(stream);
            return NULL;
        }
    }

    return stream;
}

int fclose(FILE* stream) {
    FRESULT res;

    if (!stream || !stream->used) {
        return EOF;
    }

    if (stdio_is_console(stream) || stdio_is_stdin(stream)) {
        return 0;
    }

    if (!stream->fil) {
        return EOF;
    }

    res = f_close(stream->fil);

    if (res != FR_OK) {
        stdio_free_file(stream);
        return EOF;
    }

    stdio_free_file(stream);

    return 0;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
    size_t total;
    size_t done = 0;
    UINT br = 0;
    FRESULT res;
    uint8_t* out = (uint8_t*)ptr;

    if (!ptr || !stream || !stream->used || size == 0 || count == 0) {
        return 0;
    }

    if (stdio_is_stdin(stream)) {
        stream->eof = 1;
        return 0;
    }

    if (stdio_is_console(stream)) {
        stream->error = 1;
        return 0;
    }

    if (!stream->fil) {
        stream->error = 1;
        return 0;
    }

    total = size * count;

    if (stream->has_ungot && total > 0) {
        out[0] = (uint8_t)stream->ungot;
        stream->has_ungot = 0;
        done = 1;
    }

    if (done < total) {
        res = f_read(stream->fil, out + done, (UINT)(total - done), &br);

        if (res != FR_OK) {
            stream->error = 1;
            return done / size;
        }

        done += br;
    }

    if (done < total) {
        stream->eof = 1;
    }

    return done / size;
}

static void stdio_write_console_chunk(const char* data, size_t size) {
    char buffer[129];
    size_t pos = 0;

    while (pos < size) {
        size_t chunk = size - pos;

        if (chunk > sizeof(buffer) - 1) {
            chunk = sizeof(buffer) - 1;
        }

        for (size_t i = 0; i < chunk; i++) {
            buffer[i] = data[pos + i];
        }

        buffer[chunk] = 0;

        console_write(buffer);
        uart_puts(buffer);

        pos += chunk;
    }
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
    size_t total;
    UINT bw = 0;
    FRESULT res;

    if (!ptr || !stream || !stream->used || size == 0 || count == 0) {
        return 0;
    }

    total = size * count;

    if (stdio_is_console(stream)) {
        stdio_write_console_chunk((const char*)ptr, total);
        return count;
    }

    if (stdio_is_stdin(stream)) {
        stream->error = 1;
        return 0;
    }

    if (!stream->fil) {
        stream->error = 1;
        return 0;
    }

    if (stream->append) {
        f_lseek(stream->fil, f_size(stream->fil));
    }

    res = f_write(stream->fil, ptr, (UINT)total, &bw);

    if (res != FR_OK) {
        stream->error = 1;
        return bw / size;
    }

    if (bw < total) {
        stream->error = 1;
    }

    return bw / size;
}

int fflush(FILE* stream) {
    FRESULT res;

    if (!stream) {
        return 0;
    }

    if (!stream->used) {
        return EOF;
    }

    if (stdio_is_console(stream) || stdio_is_stdin(stream)) {
        return 0;
    }

    if (!stream->fil) {
        return EOF;
    }

    res = f_sync(stream->fil);

    if (res != FR_OK) {
        stream->error = 1;
        return EOF;
    }

    return 0;
}

int fseek(FILE* stream, long offset, int whence) {
    FSIZE_t base;
    FSIZE_t target;
    FRESULT res;

    if (!stream || !stream->used || stdio_is_console(stream) || stdio_is_stdin(stream)) {
        return -1;
    }

    if (!stream->fil) {
        return -1;
    }

    if (whence == SEEK_SET) {
        if (offset < 0) {
            return -1;
        }

        target = (FSIZE_t)offset;
    } else if (whence == SEEK_CUR) {
        base = f_tell(stream->fil);

        if (offset < 0 && (FSIZE_t)(-offset) > base) {
            return -1;
        }

        target = (FSIZE_t)((long)base + offset);
    } else if (whence == SEEK_END) {
        base = f_size(stream->fil);

        if (offset < 0 && (FSIZE_t)(-offset) > base) {
            return -1;
        }

        target = (FSIZE_t)((long)base + offset);
    } else {
        return -1;
    }

    res = f_lseek(stream->fil, target);

    if (res != FR_OK) {
        stream->error = 1;
        return -1;
    }

    stream->eof = 0;
    stream->has_ungot = 0;

    return 0;
}

long ftell(FILE* stream) {
    if (!stream || !stream->used || stdio_is_console(stream) || stdio_is_stdin(stream)) {
        return -1;
    }

    if (!stream->fil) {
        return -1;
    }

    return (long)f_tell(stream->fil);
}

int feof(FILE* stream) {
    if (!stream || !stream->used) {
        return 0;
    }

    return stream->eof;
}

int ferror(FILE* stream) {
    if (!stream || !stream->used) {
        return 1;
    }

    return stream->error;
}

void clearerr(FILE* stream) {
    if (!stream || !stream->used) {
        return;
    }

    stream->eof = 0;
    stream->error = 0;
}

int fgetc(FILE* stream) {
    uint8_t ch;
    size_t got;

    if (!stream || !stream->used) {
        return EOF;
    }

    if (stream->has_ungot) {
        stream->has_ungot = 0;
        return stream->ungot;
    }

    got = fread(&ch, 1, 1, stream);

    if (got != 1) {
        return EOF;
    }

    return (int)ch;
}

int getc(FILE* stream) {
    return fgetc(stream);
}

int getchar(void) {
    return fgetc(stdin);
}

int ungetc(int c, FILE* stream) {
    if (!stream || !stream->used || c == EOF) {
        return EOF;
    }

    if (stream->has_ungot) {
        return EOF;
    }

    stream->ungot = c & 0xFF;
    stream->has_ungot = 1;
    stream->eof = 0;

    return stream->ungot;
}

char* fgets(char* s, int size, FILE* stream) {
    int c;
    int pos = 0;

    if (!s || size <= 0 || !stream) {
        return NULL;
    }

    while (pos + 1 < size) {
        c = fgetc(stream);

        if (c == EOF) {
            break;
        }

        s[pos++] = (char)c;

        if (c == '\n') {
            break;
        }
    }

    if (pos == 0) {
        return NULL;
    }

    s[pos] = 0;

    return s;
}

int fputc(int c, FILE* stream) {
    uint8_t ch = (uint8_t)c;
    size_t written;

    written = fwrite(&ch, 1, 1, stream);

    if (written != 1) {
        return EOF;
    }

    return ch;
}

int putc(int c, FILE* stream) {
    return fputc(c, stream);
}

int putchar(int c) {
    return fputc(c, stdout);
}

int fputs(const char* s, FILE* stream) {
    size_t len;

    if (!s || !stream) {
        return EOF;
    }

    len = strlen(s);

    if (fwrite(s, 1, len, stream) != len) {
        return EOF;
    }

    return 0;
}

int puts(const char* s) {
    if (fputs(s, stdout) == EOF) {
        return EOF;
    }

    if (fputc('\n', stdout) == EOF) {
        return EOF;
    }

    return 0;
}

int remove(const char* path) {
    char fat_path[NEON_STDIO_PATH_MAX];

    if (stdio_ensure_mounted() != 0) {
        return -1;
    }

    if (stdio_make_fat_path(path, fat_path, sizeof(fat_path)) != 0) {
        return -1;
    }

    if (f_unlink(fat_path) != FR_OK) {
        return -1;
    }

    return 0;
}

int rename(const char* old_path, const char* new_path) {
    char old_fat_path[NEON_STDIO_PATH_MAX];
    char new_fat_path[NEON_STDIO_PATH_MAX];

    if (stdio_ensure_mounted() != 0) {
        return -1;
    }

    if (stdio_make_fat_path(old_path, old_fat_path, sizeof(old_fat_path)) != 0) {
        return -1;
    }

    if (stdio_make_fat_path(new_path, new_fat_path, sizeof(new_fat_path)) != 0) {
        return -1;
    }

    if (f_rename(old_fat_path, new_fat_path) != FR_OK) {
        return -1;
    }

    return 0;
}

int setvbuf(FILE* stream, char* buffer, int mode, size_t size) {
    (void)stream;
    (void)buffer;
    (void)mode;
    (void)size;

    return 0;
}

void perror(const char* s) {
    if (s && s[0]) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }

    fputs("stdio error\n", stderr);
}