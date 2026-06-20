#include <stddef.h>

#include "string.h"

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
            return (int)ca - (int)cb;
        }

        if (ca == '\0') {
            return 0;
        }
    }

    return 0;
}

int strcoll(const char* a, const char* b) {
    return strcmp(a, b);
}

char* strcpy(char* dest, const char* src) {
    char* result = dest;

    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }

    *dest = '\0';

    return result;
}

char* strncpy(char* dest, const char* src, size_t size) {
    char* result = dest;
    size_t i = 0;

    while (i < size && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }

    while (i < size) {
        dest[i] = '\0';
        i++;
    }

    return result;
}

char* strchr(const char* str, int ch) {
    char target = (char)ch;

    while (*str != '\0') {
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

char* strrchr(const char* str, int ch) {
    const char* found = 0;
    char target = (char)ch;

    while (*str != '\0') {
        if (*str == target) {
            found = str;
        }

        str++;
    }

    if (target == '\0') {
        return (char*)str;
    }

    return (char*)found;
}

char* strstr(const char* text, const char* needle) {
    size_t needle_length;

    if (*needle == '\0') {
        return (char*)text;
    }

    needle_length = strlen(needle);

    while (*text != '\0') {
        if (
            *text == *needle &&
            strncmp(text, needle, needle_length) == 0
        ) {
            return (char*)text;
        }

        text++;
    }

    return 0;
}

char* strpbrk(const char* text, const char* accept) {
    while (*text != '\0') {
        if (strchr(accept, *text) != 0) {
            return (char*)text;
        }

        text++;
    }

    return 0;
}

size_t strspn(const char* text, const char* accept) {
    size_t length = 0;

    while (text[length] != '\0') {
        if (strchr(accept, text[length]) == 0) {
            break;
        }

        length++;
    }

    return length;
}

size_t strcspn(const char* text, const char* reject) {
    size_t length = 0;

    while (text[length] != '\0') {
        if (strchr(reject, text[length]) != 0) {
            break;
        }

        length++;
    }

    return length;
}