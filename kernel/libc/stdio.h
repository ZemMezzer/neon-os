#pragma once

#include <stddef.h>

#include "stdarg.h"

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ 512
#define FILENAME_MAX 260

typedef struct FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int stdio_init(void);

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);

size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);

int fflush(FILE* stream);

int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);

int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);

int fgetc(FILE* stream);
int getc(FILE* stream);
int getchar(void);

int ungetc(int c, FILE* stream);

char* fgets(char* s, int size, FILE* stream);

int fputc(int c, FILE* stream);
int putc(int c, FILE* stream);
int putchar(int c);

int fputs(const char* s, FILE* stream);
int puts(const char* s);

int remove(const char* path);
int rename(const char* old_path, const char* new_path);

int setvbuf(FILE* stream, char* buffer, int mode, size_t size);

void perror(const char* s);

int neon_vsnprintf_ptr(
    char* buffer,
    size_t size,
    const char* format,
    va_list* args
);

int neon_snprintf(
    char* buffer,
    size_t size,
    const char* format,
    ...
);

#define snprintf neon_snprintf

#define vsnprintf(buffer, size, format, args) \
    neon_vsnprintf_ptr((buffer), (size), (format), &(args))