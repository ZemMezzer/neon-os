#pragma once

/*
    Minimal bare-metal math API required by Lua core.
    It intentionally covers only the double functions currently needed.
*/

#define HUGE_VAL  __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()

double fabs(double value);
double floor(double value);
double ceil(double value);
double fmod(double x, double y);
double frexp(double value, int* exponent);
double ldexp(double value, int exponent);
double pow(double base, double exponent);

int isnan(double value);
int isinf(double value);
int isfinite(double value);
