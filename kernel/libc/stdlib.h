#pragma once

#include <stddef.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t count, size_t size);

int atoi(const char* text);
long atol(const char* text);

long strtol(const char* text, char** endptr, int base);
unsigned long strtoul(const char* text, char** endptr, int base);
double strtod(const char* text, char** endptr);

void abort(void) __attribute__((noreturn));
void exit(int status) __attribute__((noreturn));

int system(const char* command);
char* getenv(const char* name);

size_t heap_total_bytes(void);
size_t heap_used_bytes(void);
size_t heap_free_bytes(void);

int abs(int value);
long labs(long value);
long long llabs(long long value);
