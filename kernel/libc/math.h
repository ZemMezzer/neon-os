#pragma once

/*
    Bare-metal double-precision math API used by Lua and NeonOS.

    It provides the C99 functions needed by Lua's standard math library.
    The implementation is freestanding and does not depend on libm.
*/

#define HUGE_VAL  __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()

#define M_E        2.71828182845904523536
#define M_LOG2E    1.44269504088896340736
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962

double fabs(double value);
double floor(double value);
double ceil(double value);
double fmod(double x, double y);
double frexp(double value, int* exponent);
double ldexp(double value, int exponent);
double modf(double value, double* integer_part);

double sqrt(double value);
double sin(double value);
double cos(double value);
double tan(double value);
double asin(double value);
double acos(double value);
double atan(double value);
double atan2(double y, double x);

double log(double value);
double log2(double value);
double log10(double value);
double exp(double value);
double pow(double base, double exponent);

int isnan(double value);
int isinf(double value);
int isfinite(double value);
