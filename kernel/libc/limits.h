#pragma once

#include <stddef.h>

#define CHAR_BIT 8

#define SCHAR_MAX 127
#define SCHAR_MIN (-128)

#define UCHAR_MAX 255

#define SHRT_MAX 32767
#define SHRT_MIN (-32768)
#define USHRT_MAX 65535

#define INT_MAX 2147483647
#define INT_MIN (-2147483647 - 1)
#define UINT_MAX 4294967295U

#define LONG_MAX __LONG_MAX__
#define LONG_MIN (-LONG_MAX - 1L)
#define ULONG_MAX ((unsigned long)(LONG_MAX) * 2UL + 1UL)

#define SIZE_MAX ((size_t)-1)