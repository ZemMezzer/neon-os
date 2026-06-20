#include <stddef.h>

#include "errno.h"
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


char* strcat(char* dest, const char* src) {
    char* end = dest + strlen(dest);

    while (*src != '\0') {
        *end++ = *src++;
    }

    *end = '\0';

    return dest;
}


char* strncat(char* dest, const char* src, size_t size) {
    char* end = dest + strlen(dest);

    while (size != 0 && *src != '\0') {
        *end++ = *src++;
        size--;
    }

    *end = '\0';

    return dest;
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

    return NULL;
}


char* strrchr(const char* str, int ch) {
    const char* found = NULL;
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
        if (*text == *needle && strncmp(text, needle, needle_length) == 0) {
            return (char*)text;
        }

        text++;
    }

    return NULL;
}


char* strpbrk(const char* text, const char* accept) {
    while (*text != '\0') {
        if (strchr(accept, *text) != NULL) {
            return (char*)text;
        }

        text++;
    }

    return NULL;
}


size_t strspn(const char* text, const char* accept) {
    size_t length = 0;

    while (text[length] != '\0') {
        if (strchr(accept, text[length]) == NULL) {
            break;
        }

        length++;
    }

    return length;
}


size_t strcspn(const char* text, const char* reject) {
    size_t length = 0;

    while (text[length] != '\0') {
        if (strchr(reject, text[length]) != NULL) {
            break;
        }

        length++;
    }

    return length;
}


void* memchr(const void* data, int value, size_t size) {
    const unsigned char* bytes = (const unsigned char*)data;
    const unsigned char needle = (unsigned char)value;

    while (size != 0) {
        if (*bytes == needle) {
            return (void*)bytes;
        }

        bytes++;
        size--;
    }

    return NULL;
}


char* strerror(int error_number) {
    switch (error_number) {
        case 0:
            return "success";
        case EPERM:
            return "operation not permitted";
        case ENOENT:
            return "no such file or directory";
        case EIO:
            return "I/O error";
        case EBADF:
            return "bad file descriptor";
        case EAGAIN:
            return "resource temporarily unavailable";
        case ENOMEM:
            return "not enough memory";
        case EACCES:
            return "permission denied";
        case EEXIST:
            return "file exists";
        case ENOTDIR:
            return "not a directory";
        case EISDIR:
            return "is a directory";
        case EINVAL:
            return "invalid argument";
        case ENFILE:
            return "file table overflow";
        case EMFILE:
            return "too many open files";
        case ENOSPC:
            return "no space left on device";
        case EROFS:
            return "read-only file system";
        case ENAMETOOLONG:
            return "file name too long";
        case ENOSYS:
            return "function not supported";
        default:
            return "unknown error";
    }
}
