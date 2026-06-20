#include "ctype.h"

#define CTYPE_ATTR __attribute__((noinline, used, optimize("O0")))

static int ctype_valid(int c) {
    return c >= 0 && c <= 255;
}

static int ctype_lower_ascii(int c) {
    return c >= 'a' && c <= 'z';
}

static int ctype_upper_ascii(int c) {
    return c >= 'A' && c <= 'Z';
}

static int ctype_digit_ascii(int c) {
    return c >= '0' && c <= '9';
}

int islower(int c) CTYPE_ATTR;

int islower(int c) {
    return ctype_valid(c) && ctype_lower_ascii(c);
}

int isupper(int c) CTYPE_ATTR;

int isupper(int c) {
    return ctype_valid(c) && ctype_upper_ascii(c);
}

int isalpha(int c) CTYPE_ATTR;

int isalpha(int c) {
    if (!ctype_valid(c)) {
        return 0;
    }

    return ctype_lower_ascii(c) || ctype_upper_ascii(c);
}

int isdigit(int c) CTYPE_ATTR;

int isdigit(int c) {
    return ctype_valid(c) && ctype_digit_ascii(c);
}

int isxdigit(int c) CTYPE_ATTR;

int isxdigit(int c) {
    if (!ctype_valid(c)) {
        return 0;
    }

    return ctype_digit_ascii(c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int isalnum(int c) CTYPE_ATTR;

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isblank(int c) CTYPE_ATTR;

int isblank(int c) {
    return c == ' ' || c == '\t';
}

int isspace(int c) CTYPE_ATTR;

int isspace(int c) {
    return c == ' '  ||
           c == '\t' ||
           c == '\n' ||
           c == '\r' ||
           c == '\v' ||
           c == '\f';
}

int iscntrl(int c) CTYPE_ATTR;

int iscntrl(int c) {
    if (!ctype_valid(c)) {
        return 0;
    }

    return c <= 0x1F || c == 0x7F;
}

int isprint(int c) CTYPE_ATTR;

int isprint(int c) {
    return c >= 0x20 && c <= 0x7E;
}

int isgraph(int c) CTYPE_ATTR;

int isgraph(int c) {
    return c >= 0x21 && c <= 0x7E;
}

int ispunct(int c) CTYPE_ATTR;

int ispunct(int c) {
    return isgraph(c) && !isalnum(c);
}

int tolower(int c) CTYPE_ATTR;

int tolower(int c) {
    if (ctype_upper_ascii(c)) {
        return c + ('a' - 'A');
    }

    return c;
}

int toupper(int c) CTYPE_ATTR;

int toupper(int c) {
    if (ctype_lower_ascii(c)) {
        return c - ('a' - 'A');
    }

    return c;
}