#include <stddef.h>
#include <stdint.h>

#include "math.h"

#define MATH_ATTR __attribute__((noinline, used, optimize("O0")))

#define NEON_DBL_EXP_MASK  0x7FF0000000000000ULL
#define NEON_DBL_FRAC_MASK 0x000FFFFFFFFFFFFFULL
#define NEON_DBL_SIGN_MASK 0x8000000000000000ULL

#define NEON_PI          3.14159265358979323846
#define NEON_HALF_PI     1.57079632679489661923
#define NEON_QUARTER_PI  0.78539816339744830962
#define NEON_TWO_PI      6.28318530717958647692
#define NEON_TAN_PI_8    0.41421356237309504880

#define NEON_LN2         0.69314718055994530942
#define NEON_INV_LN2     1.44269504088896340736
#define NEON_LN10        2.30258509299404568402
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

static double neon_math_wrap_pi(double value) MATH_ATTR;
static double neon_math_sin_small(double value) MATH_ATTR;
static double neon_math_cos_small(double value) MATH_ATTR;
static double neon_math_atan_series(double value) MATH_ATTR;
static double neon_math_atan_positive(double value) MATH_ATTR;


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

    if (remainder != 0.0) {
        if (x > 0.0 && remainder < 0.0) {
            remainder += fabs(y);
        }
        else if (x < 0.0 && remainder > 0.0) {
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

    if (exponent != NULL) {
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

    if (exponent != NULL) {
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


double modf(double value, double* integer_part) MATH_ATTR;

double modf(double value, double* integer_part) {
    double integral;

    if (!isfinite(value) || value == 0.0) {
        if (integer_part != NULL) {
            *integer_part = value;
        }

        return value == 0.0 ? value : 0.0;
    }

    integral = value < 0.0 ? ceil(value) : floor(value);

    if (integer_part != NULL) {
        *integer_part = integral;
    }

    return value - integral;
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


double log(double value) MATH_ATTR;

double log(double value) {
    return neon_math_log(value);
}


double log2(double value) MATH_ATTR;

double log2(double value) {
    return neon_math_log(value) * NEON_INV_LN2;
}


double log10(double value) MATH_ATTR;

double log10(double value) {
    return neon_math_log(value) / NEON_LN10;
}


double exp(double value) MATH_ATTR;

double exp(double value) {
    return neon_math_exp(value);
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
    }
    else {
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

    return negative_exponent ? (1.0 / result) : result;
}


double pow(double base, double exponent) MATH_ATTR;

double pow(double base, double exponent) {
    if (exponent == 0.0 || base == 1.0) {
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

    return neon_math_exp(exponent * neon_math_log(base));
}


static double neon_math_wrap_pi(double value) {
    double turns;

    if (!isfinite(value)) {
        return neon_math_nan();
    }

    if (value > NEON_PI || value < -NEON_PI) {
        turns = floor((value + NEON_PI) / NEON_TWO_PI);
        value -= turns * NEON_TWO_PI;

        if (value > NEON_PI) {
            value -= NEON_TWO_PI;
        }
        else if (value < -NEON_PI) {
            value += NEON_TWO_PI;
        }
    }

    return value;
}


static double neon_math_sin_small(double value) {
    double value2 = value * value;

    return value * (
        1.0 + value2 * (
            -0.16666666666666666667 + value2 * (
                0.00833333333333333333 + value2 * (
                    -0.00019841269841269841 + value2 * (
                        0.00000275573192239859 + value2 *
                        -0.00000002505210838544
                    )
                )
            )
        )
    );
}


static double neon_math_cos_small(double value) {
    double value2 = value * value;

    return 1.0 + value2 * (
        -0.5 + value2 * (
            0.04166666666666666667 + value2 * (
                -0.00138888888888888889 + value2 * (
                    0.00002480158730158730 + value2 *
                    -0.00000027557319223986
                )
            )
        )
    );
}


double sin(double value) MATH_ATTR;

double sin(double value) {
    int negative = 0;

    value = neon_math_wrap_pi(value);

    if (value < 0.0) {
        value = -value;
        negative = 1;
    }

    if (value > NEON_HALF_PI) {
        value = NEON_PI - value;
    }

    value = neon_math_sin_small(value);

    return negative ? -value : value;
}


double cos(double value) MATH_ATTR;

double cos(double value) {
    int negative = 0;

    value = neon_math_wrap_pi(value);

    if (value < 0.0) {
        value = -value;
    }

    if (value > NEON_HALF_PI) {
        value = NEON_PI - value;
        negative = 1;
    }

    value = neon_math_cos_small(value);

    return negative ? -value : value;
}


double tan(double value) MATH_ATTR;

double tan(double value) {
    double cosine = cos(value);

    if (neon_math_is_nan(cosine)) {
        return cosine;
    }

    if (fabs(cosine) < 0.000000000000001) {
        return neon_math_inf(sin(value) < 0.0);
    }

    return sin(value) / cosine;
}


static double neon_math_atan_series(double value) {
    double value2 = value * value;
    double term = value;
    double sum = value;
    int denominator;

    for (denominator = 3; denominator <= 31; denominator += 2) {
        term *= -value2;
        sum += term / (double)denominator;
    }

    return sum;
}


static double neon_math_atan_positive(double value) {
    if (value > 1.0) {
        return NEON_HALF_PI - neon_math_atan_positive(1.0 / value);
    }

    if (value > NEON_TAN_PI_8) {
        return NEON_QUARTER_PI +
            neon_math_atan_series((value - 1.0) / (value + 1.0));
    }

    return neon_math_atan_series(value);
}


double atan(double value) MATH_ATTR;

double atan(double value) {
    if (neon_math_is_nan(value)) {
        return value;
    }

    if (value < 0.0) {
        return -neon_math_atan_positive(-value);
    }

    return neon_math_atan_positive(value);
}


double atan2(double y, double x) MATH_ATTR;

double atan2(double y, double x) {
    if (neon_math_is_nan(x) || neon_math_is_nan(y)) {
        return neon_math_nan();
    }

    if (neon_math_is_inf(y) && neon_math_is_inf(x)) {
        if (x > 0.0) {
            return y > 0.0 ? NEON_QUARTER_PI : -NEON_QUARTER_PI;
        }

        return y > 0.0
            ? (NEON_PI - NEON_QUARTER_PI)
            : (-NEON_PI + NEON_QUARTER_PI);
    }

    if (x > 0.0) {
        return atan(y / x);
    }

    if (x < 0.0) {
        if (y >= 0.0) {
            return atan(y / x) + NEON_PI;
        }

        return atan(y / x) - NEON_PI;
    }

    if (y > 0.0) {
        return NEON_HALF_PI;
    }

    if (y < 0.0) {
        return -NEON_HALF_PI;
    }

    return 0.0;
}


double sqrt(double value) MATH_ATTR;

double sqrt(double value) {
    double mantissa;
    double guess;
    int exponent;
    int i;

    if (neon_math_is_nan(value) || value < 0.0) {
        return neon_math_nan();
    }

    if (value == 0.0 || neon_math_is_inf(value)) {
        return value;
    }

    mantissa = frexp(value, &exponent);

    if ((exponent % 2) != 0) {
        mantissa *= 2.0;
        exponent--;
    }

    guess = 0.41731 + 0.59016 * mantissa;

    for (i = 0; i < 8; i++) {
        guess = 0.5 * (guess + mantissa / guess);
    }

    return ldexp(guess, exponent / 2);
}


double asin(double value) MATH_ATTR;

double asin(double value) {
    if (neon_math_is_nan(value) || value < -1.0 || value > 1.0) {
        return neon_math_nan();
    }

    if (value == 1.0) {
        return NEON_HALF_PI;
    }

    if (value == -1.0) {
        return -NEON_HALF_PI;
    }

    return atan2(value, sqrt(1.0 - value * value));
}


double acos(double value) MATH_ATTR;

double acos(double value) {
    if (neon_math_is_nan(value) || value < -1.0 || value > 1.0) {
        return neon_math_nan();
    }

    return NEON_HALF_PI - asin(value);
}
