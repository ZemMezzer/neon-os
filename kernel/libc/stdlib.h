#pragma once

#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t count, size_t size);

void abort(void) __attribute__((noreturn));

size_t heap_total_bytes(void);
size_t heap_used_bytes(void);
size_t heap_free_bytes(void);