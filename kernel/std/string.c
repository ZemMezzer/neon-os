#include <stddef.h>

size_t strlen(const char* str) {
    size_t length = 0;

    while (str[length] != '\0') {
        length++;
    }

    return length;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return (unsigned char)*a - (unsigned char)*b;
        }

        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t size) {
    for (size_t i = 0; i < size; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];

        if (ca != cb) {
            return ca - cb;
        }

        if (ca == '\0') {
            return 0;
        }
    }

    return 0;
}

char* strchr(const char* str, int ch) {
    char target = (char)ch;

    while (*str) {
        if (*str == target) {
            return (char*)str;
        }

        str++;
    }

    if (target == '\0') {
        return (char*)str;
    }

    return 0;
}