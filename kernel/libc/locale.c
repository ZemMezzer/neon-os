#include "locale.h"

#define LOCALE_ATTR __attribute__((noinline, used, optimize("O0")))

static char locale_empty[] = "";
static char locale_dot[] = ".";
static char locale_c_name[] = "C";

static struct lconv neon_c_locale = {
    locale_dot,
    locale_empty,
    locale_empty,
    locale_empty,
    locale_empty,
    locale_empty,
    locale_empty,
    locale_empty,
    locale_empty,
    locale_empty,
    127,
    127,
    127,
    127,
    127,
    127,
    127,
    127,
    127,
    127,
    127,
    127
};

struct lconv* localeconv(void) LOCALE_ATTR;

struct lconv* localeconv(void) {
    return &neon_c_locale;
}

char* setlocale(int category, const char* locale) LOCALE_ATTR;

char* setlocale(int category, const char* locale) {
    (void)category;

    if (!locale) {
        return locale_c_name;
    }

    if (
        locale[0] == '\0' ||
        (locale[0] == 'C' && locale[1] == '\0')
    ) {
        return locale_c_name;
    }

    return 0;
}
