#pragma once

#include <stddef.h>

void* memset(void* ptr, int value, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
void* memmove(void* dest, const void* src, size_t size);
int memcmp(const void* a, const void* b, size_t size);

size_t strlen(const char* str);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t size);
char* strchr(const char* str, int ch);