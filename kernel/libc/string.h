#pragma once

#include <stddef.h>

void* memset(void* ptr, int value, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
void* memmove(void* dest, const void* src, size_t size);
int memcmp(const void* a, const void* b, size_t size);
void* memchr(const void* data, int value, size_t size);

size_t strlen(const char* str);

int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t size);
int strcoll(const char* a, const char* b);

char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t size);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t size);

char* strchr(const char* str, int ch);
char* strrchr(const char* str, int ch);
char* strstr(const char* text, const char* needle);
char* strpbrk(const char* text, const char* accept);

size_t strspn(const char* text, const char* accept);
size_t strcspn(const char* text, const char* reject);

char* strerror(int error_number);
