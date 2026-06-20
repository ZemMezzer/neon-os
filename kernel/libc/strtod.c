#include <stddef.h>
#include <stdint.h>

#include "ctype.h"
#include "errno.h"
#include "float.h"
#include "stdlib.h"

#define STRTOD_ATTR __attribute__((noinline, used, optimize("O0")))
#define STRTOD_LOCAL __attribute__((noinline, optimize("O0")))

#define STRTOD_SIGNIFICANT_DIGITS 18
#define STRTOD_EXPONENT_LIMIT 100000

static int strtod_is_digit(int c) STRTOD_LOCAL;

static int strtod_is_digit(int c) {
    return c >= '0' && c <= '9';
}

static const char* strtod_skip_space(const char* text) STRTOD_LOCAL;

static const char* strtod_skip_space(const char* text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

static int strtod_saturating_add(int left, int right) STRTOD_LOCAL;

static int strtod_saturating_add(int left, int right) {
    int64_t result = (int64_t)left + (int64_t)right;

    if (result > STRTOD_EXPONENT_LIMIT) {
        return STRTOD_EXPONENT_LIMIT;
    }

    if (result < -STRTOD_EXPONENT_LIMIT) {
        return -STRTOD_EXPONENT_LIMIT;
    }

    return (int)result;
}

static int strtod_parse_exponent(
    const char** text,
    int* exponent
) STRTOD_LOCAL;

static int strtod_parse_exponent(
    const char** text,
    int* exponent
) {
    const char* current;
    int negative = 0;
    int value = 0;

    current = *text;

    if (*current != 'e' && *current != 'E') {
        return 0;
    }

    current++;

    if (*current == '-') {
        negative = 1;
        current++;
    } else if (*current == '+') {
        current++;
    }

    if (!strtod_is_digit((unsigned char)*current)) {
        return 0;
    }

    while (strtod_is_digit((unsigned char)*current)) {
        int digit = *current - '0';

        if (value < STRTOD_EXPONENT_LIMIT) {
            value = value * 10 + digit;

            if (value > STRTOD_EXPONENT_LIMIT) {
                value = STRTOD_EXPONENT_LIMIT;
            }
        }

        current++;
    }

    *text = current;
    *exponent = negative ? -value : value;

    return 1;
}

static double strtod_scale_decimal(
    double value,
    int exponent,
    int* range_error
) STRTOD_LOCAL;

static double strtod_scale_decimal(
    double value,
    int exponent,
    int* range_error
) {
    if (value == 0.0) {
        return 0.0;
    }

    if (exponent > 400) {
        *range_error = 1;
        return DBL_MAX;
    }

    if (exponent < -400) {
        *range_error = 1;
        return 0.0;
    }

    if (exponent > 0) {
        for (int i = 0; i < exponent; i++) {
            if (value > DBL_MAX / 10.0) {
                *range_error = 1;
                return DBL_MAX;
            }

            value *= 10.0;
        }
    } else if (exponent < 0) {
        for (int i = 0; i < -exponent; i++) {
            value /= 10.0;

            if (value == 0.0) {
                *range_error = 1;
                return 0.0;
            }
        }
    }

    return value;
}

double strtod(
    const char* text,
    char** endptr
) STRTOD_ATTR;

double strtod(
    const char* text,
    char** endptr
) {
    const char* current;
    const char* original;
    double value = 0.0;
    int negative = 0;
    int any_digits = 0;
    int after_decimal_point = 0;
    int significant_started = 0;
    int kept_digits = 0;
    int dropped_digits = 0;
    int fractional_digits = 0;
    int explicit_exponent = 0;
    int scale_exponent;
    int range_error = 0;

    if (!text) {
        if (endptr) {
            *endptr = 0;
        }

        errno = EINVAL;
        return 0.0;
    }

    original = text;
    current = strtod_skip_space(text);

    if (*current == '-') {
        negative = 1;
        current++;
    } else if (*current == '+') {
        current++;
    }

    while (strtod_is_digit((unsigned char)*current)) {
        int digit = *current - '0';

        any_digits = 1;

        if (digit != 0 || significant_started) {
            significant_started = 1;

            if (kept_digits < STRTOD_SIGNIFICANT_DIGITS) {
                value = value * 10.0 + (double)digit;
                kept_digits++;
            } else if (dropped_digits < STRTOD_EXPONENT_LIMIT) {
                dropped_digits++;
            }
        }

        current++;
    }

    if (*current == '.') {
        after_decimal_point = 1;
        current++;

        while (strtod_is_digit((unsigned char)*current)) {
            int digit = *current - '0';

            any_digits = 1;

            if (fractional_digits < STRTOD_EXPONENT_LIMIT) {
                fractional_digits++;
            }

            if (digit != 0 || significant_started) {
                significant_started = 1;

                if (kept_digits < STRTOD_SIGNIFICANT_DIGITS) {
                    value = value * 10.0 + (double)digit;
                    kept_digits++;
                } else if (dropped_digits < STRTOD_EXPONENT_LIMIT) {
                    dropped_digits++;
                }
            }

            current++;
        }
    }

    (void)after_decimal_point;

    if (!any_digits) {
        if (endptr) {
            *endptr = (char*)original;
        }

        return 0.0;
    }

    {
        const char* exponent_start = current;

        if (!strtod_parse_exponent(&current, &explicit_exponent)) {
            current = exponent_start;
            explicit_exponent = 0;
        }
    }

    scale_exponent = explicit_exponent;

    scale_exponent = strtod_saturating_add(
        scale_exponent,
        dropped_digits
    );

    scale_exponent = strtod_saturating_add(
        scale_exponent,
        -fractional_digits
    );

    value = strtod_scale_decimal(
        value,
        scale_exponent,
        &range_error
    );

    if (range_error) {
        errno = ERANGE;
    }

    if (endptr) {
        *endptr = (char*)current;
    }

    return negative ? -value : value;
}
