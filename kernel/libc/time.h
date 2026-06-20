#pragma once

#include <stddef.h>

typedef long long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000L

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

clock_t clock(void);
time_t time(time_t* output);
double difftime(time_t end, time_t beginning);

struct tm* gmtime(const time_t* value);
struct tm* localtime(const time_t* value);
time_t mktime(struct tm* value);

size_t strftime(
    char* output,
    size_t output_size,
    const char* format,
    const struct tm* time_info
);

void neon_time_set_epoch(time_t seconds_since_1970);
void neon_time_advance_seconds(time_t seconds);
