#include <stddef.h>

void* memset(void* ptr, int value, size_t size)
    __attribute__((noinline, used, optimize("O0")));

void* memcpy(void* dest, const void* src, size_t size)
    __attribute__((noinline, used, optimize("O0")));

void* memmove(void* dest, const void* src, size_t size)
    __attribute__((noinline, used, optimize("O0")));

int memcmp(const void* a, const void* b, size_t size)
    __attribute__((noinline, used, optimize("O0")));

void* memset(void* ptr, int value, size_t size) {
    volatile unsigned char* p = (volatile unsigned char*)ptr;

    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)value;
    }

    return ptr;
}

void* memcpy(void* dest, const void* src, size_t size) {
    volatile unsigned char* d = (volatile unsigned char*)dest;
    const volatile unsigned char* s = (const volatile unsigned char*)src;

    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }

    return dest;
}

void* memmove(void* dest, const void* src, size_t size) {
    volatile unsigned char* d = (volatile unsigned char*)dest;
    const volatile unsigned char* s = (const volatile unsigned char*)src;

    if (d == s || size == 0) {
        return dest;
    }

    if (d < s) {
        for (size_t i = 0; i < size; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = size; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

int memcmp(const void* a, const void* b, size_t size) {
    const volatile unsigned char* x = (const volatile unsigned char*)a;
    const volatile unsigned char* y = (const volatile unsigned char*)b;

    for (size_t i = 0; i < size; i++) {
        if (x[i] != y[i]) {
            return (int)x[i] - (int)y[i];
        }
    }

    return 0;
}