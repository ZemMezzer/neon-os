#include <stddef.h>

#include "ctype.h"
#include "errno.h"
#include "limits.h"
#include "stdlib.h"

#define STRTOL_ATTR __attribute__((noinline, used, optimize("O0")))

static int strto_digit_value(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }

    return -1;
}

static const char* strto_skip_space(const char* text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

static int strto_has_hex_prefix(const char* text) {
    int digit;

    if (text[0] != '0') {
        return 0;
    }

    if (text[1] != 'x' && text[1] != 'X') {
        return 0;
    }

    digit = strto_digit_value((unsigned char)text[2]);

    return digit >= 0 && digit < 16;
}

static int strto_prepare(
    const char* text,
    const char** number_start,
    int* negative,
    int* base
) {
    const char* current;

    if (!text || !number_start || !negative || !base) {
        return 0;
    }

    if (*base != 0 && (*base < 2 || *base > 36)) {
        errno = EINVAL;
        return 0;
    }

    current = strto_skip_space(text);

    *negative = 0;

    if (*current == '-') {
        *negative = 1;
        current++;
    } else if (*current == '+') {
        current++;
    }

    if (*base == 0) {
        if (strto_has_hex_prefix(current)) {
            *base = 16;
            current += 2;
        } else if (*current == '0') {
            *base = 8;
        } else {
            *base = 10;
        }
    } else if (*base == 16 && strto_has_hex_prefix(current)) {
        current += 2;
    }

    *number_start = current;

    return 1;
}

long strtol(const char* text, char** endptr, int base) STRTOL_ATTR;

long strtol(const char* text, char** endptr, int base) {
    const char* current;
    const char* number_start;
    unsigned long value = 0;
    unsigned long limit;
    unsigned long cutoff;
    unsigned long cutlim;
    int negative;
    int any_digits = 0;
    int overflow = 0;

    if (!text) {
        if (endptr) {
            *endptr = 0;
        }

        errno = EINVAL;
        return 0;
    }

    if (!strto_prepare(text, &number_start, &negative, &base)) {
        if (endptr) {
            *endptr = (char*)text;
        }

        return 0;
    }

    limit = negative
        ? (unsigned long)LONG_MAX + 1UL
        : (unsigned long)LONG_MAX;

    cutoff = limit / (unsigned long)base;
    cutlim = limit % (unsigned long)base;

    current = number_start;

    while (*current != '\0') {
        int digit = strto_digit_value((unsigned char)*current);

        if (digit < 0 || digit >= base) {
            break;
        }

        any_digits = 1;

        if (
            value > cutoff ||
            (value == cutoff && (unsigned long)digit > cutlim)
        ) {
            overflow = 1;
        } else {
            value = value * (unsigned long)base + (unsigned long)digit;
        }

        current++;
    }

    if (endptr) {
        *endptr = (char*)(any_digits ? current : text);
    }

    if (!any_digits) {
        return 0;
    }

    if (overflow) {
        errno = ERANGE;

        return negative ? LONG_MIN : LONG_MAX;
    }

    if (negative) {
        if (value == (unsigned long)LONG_MAX + 1UL) {
            return LONG_MIN;
        }

        return -(long)value;
    }

    return (long)value;
}

unsigned long strtoul(
    const char* text,
    char** endptr,
    int base
) STRTOL_ATTR;

unsigned long strtoul(
    const char* text,
    char** endptr,
    int base
) {
    const char* current;
    const char* number_start;
    unsigned long value = 0;
    unsigned long cutoff;
    unsigned long cutlim;
    int negative;
    int any_digits = 0;
    int overflow = 0;

    if (!text) {
        if (endptr) {
            *endptr = 0;
        }

        errno = EINVAL;
        return 0;
    }

    if (!strto_prepare(text, &number_start, &negative, &base)) {
        if (endptr) {
            *endptr = (char*)text;
        }

        return 0;
    }

    cutoff = ULONG_MAX / (unsigned long)base;
    cutlim = ULONG_MAX % (unsigned long)base;

    current = number_start;

    while (*current != '\0') {
        int digit = strto_digit_value((unsigned char)*current);

        if (digit < 0 || digit >= base) {
            break;
        }

        any_digits = 1;

        if (
            value > cutoff ||
            (value == cutoff && (unsigned long)digit > cutlim)
        ) {
            overflow = 1;
        } else {
            value = value * (unsigned long)base + (unsigned long)digit;
        }

        current++;
    }

    if (endptr) {
        *endptr = (char*)(any_digits ? current : text);
    }

    if (!any_digits) {
        return 0;
    }

    if (overflow) {
        errno = ERANGE;
        return ULONG_MAX;
    }

    if (negative) {
        return 0UL - value;
    }

    return value;
}

int atoi(const char* text) STRTOL_ATTR;

int atoi(const char* text) {
    return (int)strtol(text, 0, 10);
}

long atol(const char* text) STRTOL_ATTR;

long atol(const char* text) {
    return strtol(text, 0, 10);
}