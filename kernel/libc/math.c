#include <stdint.h>

#include "math.h"

#define MATH_ATTR __attribute__((noinline, used, optimize("O0")))

#define NEON_DBL_EXP_MASK  0x7FF0000000000000ULL
#define NEON_DBL_FRAC_MASK 0x000FFFFFFFFFFFFFULL
#define NEON_DBL_SIGN_MASK 0x8000000000000000ULL

#define NEON_LN2 0.69314718055994530942
#define NEON_INV_LN2 1.44269504088896340736
#define NEON_LOG_DBL_MAX 709.78271289338397310
#define NEON_LOG_DBL_MIN -745.13321910194110842

typedef union NeonDoubleBits {
    double value;
    uint64_t bits;
} NeonDoubleBits; 

static int neon_math_is_nan(double value) MATH_ATTR;
static int neon_math_is_inf(double value) MATH_ATTR;
static double neon_math_nan(void) MATH_ATTR;
static double neon_math_inf(int negative) MATH_ATTR;
static double neon_math_log(double value) MATH_ATTR;
static double neon_math_exp(double value) MATH_ATTR;
static int neon_math_is_integer(double value) MATH_ATTR;
static double neon_math_integer_pow(double base, long long exponent) MATH_ATTR;

static int neon_math_is_nan(double value) {
    NeonDoubleBits number;

    number.value = value;

    return (
        (number.bits & NEON_DBL_EXP_MASK) == NEON_DBL_EXP_MASK &&
        (number.bits & NEON_DBL_FRAC_MASK) != 0
    );
}

static int neon_math_is_inf(double value) {
    NeonDoubleBits number;

    number.value = value;

    return (number.bits & ~NEON_DBL_SIGN_MASK) == NEON_DBL_EXP_MASK;
}

static double neon_math_nan(void) {
    NeonDoubleBits number;

    number.bits = NEON_DBL_EXP_MASK | 1ULL;

    return number.value;
}

static double neon_math_inf(int negative) {
    NeonDoubleBits number;

    number.bits = NEON_DBL_EXP_MASK;

    if (negative) {
        number.bits |= NEON_DBL_SIGN_MASK;
    }

    return number.value;
}

int isnan(double value) MATH_ATTR;

int isnan(double value) {
    return neon_math_is_nan(value);
}

int isinf(double value) MATH_ATTR;

int isinf(double value) {
    if (!neon_math_is_inf(value)) {
        return 0;
    }

    return value < 0.0 ? -1 : 1;
}

int isfinite(double value) MATH_ATTR;

int isfinite(double value) {
    return !neon_math_is_nan(value) && !neon_math_is_inf(value);
}

double fabs(double value) MATH_ATTR;

double fabs(double value) {
    NeonDoubleBits number;

    number.value = value;
    number.bits &= ~NEON_DBL_SIGN_MASK;

    return number.value;
}

double floor(double value) MATH_ATTR;

double floor(double value) {
    long long truncated;
    double integer_part;

    if (!isfinite(value) || value == 0.0) {
        return value;
    }

    /*
        Outside this range, every representable double is already integral
        at the granularity Lua can observe.
    */
    if (
        value >= 9223372036854775808.0 ||
        value <= -9223372036854775808.0
    ) {
        return value;
    }

    truncated = (long long)value;
    integer_part = (double)truncated;

    if (value < 0.0 && integer_part != value) {
        return integer_part - 1.0;
    }

    return integer_part;
}

double ceil(double value) MATH_ATTR;

double ceil(double value) {
    long long truncated;
    double integer_part;

    if (!isfinite(value) || value == 0.0) {
        return value;
    }

    if (
        value >= 9223372036854775808.0 ||
        value <= -9223372036854775808.0
    ) {
        return value;
    }

    truncated = (long long)value;
    integer_part = (double)truncated;

    if (value > 0.0 && integer_part != value) {
        return integer_part + 1.0;
    }

    return integer_part;
}

double fmod(double x, double y) MATH_ATTR;

double fmod(double x, double y) {
    double quotient;
    double integer_quotient;
    double remainder;

    if (
        y == 0.0 ||
        neon_math_is_nan(x) ||
        neon_math_is_nan(y) ||
        neon_math_is_inf(x)
    ) {
        return neon_math_nan();
    }

    if (neon_math_is_inf(y) || x == 0.0) {
        return x;
    }

    quotient = x / y;
    integer_quotient = quotient < 0.0 ? ceil(quotient) : floor(quotient);
    remainder = x - integer_quotient * y;

    /*
        Keep the sign convention of C fmod: the remainder follows x.
        The corrections also help with one rounding step around boundaries.
    */
    if (remainder != 0.0) {
        if (x > 0.0 && remainder < 0.0) {
            remainder += fabs(y);
        } else if (x < 0.0 && remainder > 0.0) {
            remainder -= fabs(y);
        }
    }

    return remainder;
}

double frexp(double value, int* exponent) MATH_ATTR;

double frexp(double value, int* exponent) {
    int local_exponent = 0;
    double magnitude;
    int negative = value < 0.0;

    if (exponent) {
        *exponent = 0;
    }

    if (value == 0.0 || !isfinite(value)) {
        return value;
    }

    magnitude = fabs(value);

    while (magnitude >= 1.0) {
        magnitude *= 0.5;
        local_exponent++;
    }

    while (magnitude < 0.5) {
        magnitude *= 2.0;
        local_exponent--;
    }

    if (exponent) {
        *exponent = local_exponent;
    }

    return negative ? -magnitude : magnitude;
}

double ldexp(double value, int exponent) MATH_ATTR;

double ldexp(double value, int exponent) {
    if (value == 0.0 || !isfinite(value)) {
        return value;
    }

    if (exponent > 4096) {
        return neon_math_inf(value < 0.0);
    }

    if (exponent < -4096) {
        return value < 0.0 ? -0.0 : 0.0;
    }

    while (exponent > 0) {
        value *= 2.0;

        if (neon_math_is_inf(value)) {
            return value;
        }

        exponent--;
    }

    while (exponent < 0) {
        value *= 0.5;

        if (value == 0.0) {
            return value;
        }

        exponent++;
    }

    return value;
}

static double neon_math_log(double value) {
    double mantissa;
    double z;
    double z2;
    double term;
    double sum;
    int exponent;
    int denominator;

    if (value < 0.0 || neon_math_is_nan(value)) {
        return neon_math_nan();
    }

    if (value == 0.0) {
        return neon_math_inf(1);
    }

    if (neon_math_is_inf(value)) {
        return value;
    }

    mantissa = frexp(value, &exponent);
    mantissa *= 2.0;
    exponent--;

    z = (mantissa - 1.0) / (mantissa + 1.0);
    z2 = z * z;
    term = z;
    sum = z;

    /*
        atanh-series: ln(m) = 2 * (z + z^3/3 + z^5/5 + ...)
        30 terms are enough for the Lua use cases here.
    */
    for (denominator = 3; denominator <= 59; denominator += 2) {
        term *= z2;
        sum += term / (double)denominator;
    }

    return 2.0 * sum + (double)exponent * NEON_LN2;
}

static double neon_math_exp(double value) {
    int exponent;
    double remainder;
    double term;
    double sum;
    int i;

    if (neon_math_is_nan(value)) {
        return value;
    }

    if (value > NEON_LOG_DBL_MAX) {
        return neon_math_inf(0);
    }

    if (value < NEON_LOG_DBL_MIN) {
        return 0.0;
    }

    exponent = (int)floor(value * NEON_INV_LN2);
    remainder = value - (double)exponent * NEON_LN2;

    term = 1.0;
    sum = 1.0;

    for (i = 1; i <= 30; i++) {
        term *= remainder / (double)i;
        sum += term;
    }

    return ldexp(sum, exponent);
}

static int neon_math_is_integer(double value) {
    if (!isfinite(value)) {
        return 0;
    }

    return floor(value) == value;
}

static double neon_math_integer_pow(double base, long long exponent) {
    unsigned long long power;
    double result = 1.0;
    int negative_exponent = exponent < 0;

    if (negative_exponent) {
        power = (unsigned long long)(-(exponent + 1LL)) + 1ULL;
    } else {
        power = (unsigned long long)exponent;
    }

    while (power != 0) {
        if (power & 1ULL) {
            result *= base;
        }

        power >>= 1;

        if (power != 0) {
            base *= base;
        }
    }

    if (negative_exponent) {
        return 1.0 / result;
    }

    return result;
}

double pow(double base, double exponent) MATH_ATTR;

double pow(double base, double exponent) {
    double magnitude;
    double result;

    if (exponent == 0.0) {
        return 1.0;
    }

    if (base == 1.0) {
        return 1.0;
    }

    if (neon_math_is_nan(base) || neon_math_is_nan(exponent)) {
        return neon_math_nan();
    }

    if (
        neon_math_is_integer(exponent) &&
        exponent > -9223372036854775808.0 &&
        exponent < 9223372036854775808.0
    ) {
        return neon_math_integer_pow(base, (long long)exponent);
    }

    if (base < 0.0) {
        return neon_math_nan();
    }

    magnitude = neon_math_exp(exponent * neon_math_log(base));

    result = magnitude;

    return result;
}
