#include <stddef.h>
#include <stdint.h>

#include "time.h"

#define SECONDS_PER_DAY 86400LL

/*
    The ARM64 generic counter is a monotonic hardware timer provided by QEMU
    and by Raspberry Pi-class ARM platforms. Reading it does not require an
    IRQ handler, so Lua os.clock() can advance immediately even before the
    kernel has a scheduler or timer interrupts.
*/
static uint64_t neon_timer_frequency = 0;
static uint64_t neon_timer_boot_counter = 0;
static uint64_t neon_timer_epoch_counter = 0;
static int neon_timer_ready = 0;

static time_t neon_epoch_seconds = 0;
static struct tm neon_tm_storage;


static uint64_t neon_timer_read_counter(void) {
#if defined(__aarch64__)
    uint64_t value;

    asm volatile("mrs %0, cntpct_el0" : "=r"(value));
    return value;
#else
    return 0;
#endif
}


static uint64_t neon_timer_read_frequency(void) {
#if defined(__aarch64__)
    uint64_t value;

    asm volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
#else
    return 0;
#endif
}


static void neon_timer_ensure_ready(void) {
    if (neon_timer_ready) {
        return;
    }

    neon_timer_frequency = neon_timer_read_frequency();

    /*
        cntfrq_el0 is defined by the ARM generic timer. The fallback only
        keeps this libc usable on a non-ARM test build; real NeonOS targets
        should always report a non-zero counter frequency.
    */
    if (neon_timer_frequency == 0) {
        neon_timer_frequency = 1000000ULL;
    }

    neon_timer_boot_counter = neon_timer_read_counter();
    neon_timer_epoch_counter = neon_timer_boot_counter;
    neon_timer_ready = 1;
}


static uint64_t neon_timer_elapsed_ticks(uint64_t base_counter) {
    uint64_t now;

    neon_timer_ensure_ready();

    now = neon_timer_read_counter();
    return now - base_counter;
}


static time_t neon_timer_elapsed_seconds(uint64_t base_counter) {
    return (time_t)(
        neon_timer_elapsed_ticks(base_counter) / neon_timer_frequency
    );
}


static clock_t neon_timer_ticks_to_clock(uint64_t ticks) {
    uint64_t whole_seconds;
    uint64_t remainder;

    neon_timer_ensure_ready();

    /*
        Split the conversion into seconds and a remainder so multiplication by
        CLOCKS_PER_SEC cannot overflow after a few days of uptime.
    */
    whole_seconds = ticks / neon_timer_frequency;
    remainder = ticks % neon_timer_frequency;

    return (clock_t)(
        whole_seconds * (uint64_t)CLOCKS_PER_SEC +
        (remainder * (uint64_t)CLOCKS_PER_SEC) / neon_timer_frequency
    );
}

static const char* const weekday_short[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* const weekday_long[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

static const char* const month_short[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char* const month_long[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static int neon_is_leap_year(int year) {
    return (
        (year % 4 == 0) &&
        ((year % 100 != 0) || (year % 400 == 0))
    );
}

static int neon_days_in_month(int year, int month_zero_based) {
    static const int normal_year_days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month_zero_based == 1 && neon_is_leap_year(year)) {
        return 29;
    }

    return normal_year_days[month_zero_based];
}

/* Days since 1970-01-01. Civil date algorithm by Hinnant, integer-only. */
static long long neon_days_from_civil(int year, unsigned month, unsigned day) {
    int era;
    unsigned year_of_era;
    unsigned day_of_year;
    unsigned day_of_era;
    int adjusted_month;

    year -= month <= 2;
    era = (year >= 0 ? year : year - 399) / 400;
    year_of_era = (unsigned)(year - era * 400);
    adjusted_month = (int)month + (month > 2 ? -3 : 9);
    day_of_year = (unsigned)((153 * adjusted_month + 2) / 5) + day - 1;
    day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;

    return (long long)era * 146097LL + (long long)day_of_era - 719468LL;
}

static void neon_civil_from_days(
    long long days,
    int* out_year,
    unsigned* out_month,
    unsigned* out_day
) {
    long long era;
    unsigned day_of_era;
    unsigned year_of_era;
    int year;
    unsigned day_of_year;
    unsigned month_part;
    unsigned day;
    unsigned month;

    days += 719468LL;
    era = (days >= 0 ? days : days - 146096LL) / 146097LL;
    day_of_era = (unsigned)(days - era * 146097LL);
    year_of_era = (day_of_era - day_of_era / 1460 +
        day_of_era / 36524 - day_of_era / 146096) / 365;
    year = (int)year_of_era + (int)(era * 400);
    day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    month_part = (5 * day_of_year + 2) / 153;
    day = day_of_year - (153 * month_part + 2) / 5 + 1;
    month = month_part + (month_part < 10 ? 3 : (unsigned)-9);
    year += month <= 2;

    *out_year = year;
    *out_month = month;
    *out_day = day;
}

static int neon_day_of_year(int year, int month_zero_based, int day) {
    int total = 0;

    for (int month = 0; month < month_zero_based; month++) {
        total += neon_days_in_month(year, month);
    }

    return total + day - 1;
}

static struct tm* neon_break_time(time_t value) {
    long long days = value / SECONDS_PER_DAY;
    long long seconds = value % SECONDS_PER_DAY;
    int year;
    unsigned month;
    unsigned day;

    if (seconds < 0) {
        seconds += SECONDS_PER_DAY;
        days--;
    }

    neon_civil_from_days(days, &year, &month, &day);

    neon_tm_storage.tm_year = year - 1900;
    neon_tm_storage.tm_mon = (int)month - 1;
    neon_tm_storage.tm_mday = (int)day;
    neon_tm_storage.tm_hour = (int)(seconds / 3600);
    seconds %= 3600;
    neon_tm_storage.tm_min = (int)(seconds / 60);
    neon_tm_storage.tm_sec = (int)(seconds % 60);

    neon_tm_storage.tm_wday = (int)((days + 4) % 7);
    if (neon_tm_storage.tm_wday < 0) {
        neon_tm_storage.tm_wday += 7;
    }

    neon_tm_storage.tm_yday = neon_day_of_year(
        year,
        neon_tm_storage.tm_mon,
        neon_tm_storage.tm_mday
    );
    neon_tm_storage.tm_isdst = 0;

    return &neon_tm_storage;
}

static int neon_append_char(char* output, size_t output_size, size_t* position, char value) {
    if (*position + 1 >= output_size) {
        return -1;
    }

    output[*position] = value;
    *position += 1;
    output[*position] = '\0';

    return 0;
}

static int neon_append_string(
    char* output,
    size_t output_size,
    size_t* position,
    const char* text
) {
    while (*text != '\0') {
        if (neon_append_char(output, output_size, position, *text++) != 0) {
            return -1;
        }
    }

    return 0;
}

static int neon_append_decimal(
    char* output,
    size_t output_size,
    size_t* position,
    int value,
    int width,
    char padding
) {
    char digits[16];
    int count = 0;

    if (value < 0) {
        if (neon_append_char(output, output_size, position, '-') != 0) {
            return -1;
        }

        value = -value;
    }

    do {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && count < (int)sizeof(digits));

    while (count < width) {
        if (neon_append_char(output, output_size, position, padding) != 0) {
            return -1;
        }

        width--;
    }

    while (count != 0) {
        if (neon_append_char(output, output_size, position, digits[--count]) != 0) {
            return -1;
        }
    }

    return 0;
}

static int neon_append_strftime_item(
    char* output,
    size_t output_size,
    size_t* position,
    char specifier,
    const struct tm* value
) {
    int year = value->tm_year + 1900;
    int hour12;

    switch (specifier) {
        case '%':
            return neon_append_char(output, output_size, position, '%');
        case 'Y':
            return neon_append_decimal(output, output_size, position, year, 4, '0');
        case 'y':
            return neon_append_decimal(output, output_size, position, year % 100, 2, '0');
        case 'm':
            return neon_append_decimal(output, output_size, position, value->tm_mon + 1, 2, '0');
        case 'd':
            return neon_append_decimal(output, output_size, position, value->tm_mday, 2, '0');
        case 'e':
            return neon_append_decimal(output, output_size, position, value->tm_mday, 2, ' ');
        case 'H':
            return neon_append_decimal(output, output_size, position, value->tm_hour, 2, '0');
        case 'I':
            hour12 = value->tm_hour % 12;
            if (hour12 == 0) {
                hour12 = 12;
            }
            return neon_append_decimal(output, output_size, position, hour12, 2, '0');
        case 'M':
            return neon_append_decimal(output, output_size, position, value->tm_min, 2, '0');
        case 'S':
            return neon_append_decimal(output, output_size, position, value->tm_sec, 2, '0');
        case 'j':
            return neon_append_decimal(output, output_size, position, value->tm_yday + 1, 3, '0');
        case 'w':
            return neon_append_decimal(output, output_size, position, value->tm_wday, 1, '0');
        case 'a':
            return neon_append_string(output, output_size, position, weekday_short[value->tm_wday]);
        case 'A':
            return neon_append_string(output, output_size, position, weekday_long[value->tm_wday]);
        case 'b':
        case 'h':
            return neon_append_string(output, output_size, position, month_short[value->tm_mon]);
        case 'B':
            return neon_append_string(output, output_size, position, month_long[value->tm_mon]);
        case 'p':
            return neon_append_string(
                output,
                output_size,
                position,
                value->tm_hour < 12 ? "AM" : "PM"
            );
        case 'z':
            return neon_append_string(output, output_size, position, "+0000");
        case 'Z':
            return neon_append_string(output, output_size, position, "UTC");
        case 'x':
            if (neon_append_decimal(output, output_size, position, value->tm_mon + 1, 2, '0') != 0 ||
                neon_append_char(output, output_size, position, '/') != 0 ||
                neon_append_decimal(output, output_size, position, value->tm_mday, 2, '0') != 0 ||
                neon_append_char(output, output_size, position, '/') != 0) {
                return -1;
            }
            return neon_append_decimal(output, output_size, position, year % 100, 2, '0');
        case 'X':
            if (neon_append_decimal(output, output_size, position, value->tm_hour, 2, '0') != 0 ||
                neon_append_char(output, output_size, position, ':') != 0 ||
                neon_append_decimal(output, output_size, position, value->tm_min, 2, '0') != 0 ||
                neon_append_char(output, output_size, position, ':') != 0) {
                return -1;
            }
            return neon_append_decimal(output, output_size, position, value->tm_sec, 2, '0');
        case 'c':
            if (neon_append_string(output, output_size, position, weekday_short[value->tm_wday]) != 0 ||
                neon_append_char(output, output_size, position, ' ') != 0 ||
                neon_append_string(output, output_size, position, month_short[value->tm_mon]) != 0 ||
                neon_append_char(output, output_size, position, ' ') != 0 ||
                neon_append_decimal(output, output_size, position, value->tm_mday, 2, ' ') != 0 ||
                neon_append_char(output, output_size, position, ' ') != 0 ||
                neon_append_decimal(output, output_size, position, value->tm_hour, 2, '0') != 0 ||
                neon_append_char(output, output_size, position, ':') != 0 ||
                neon_append_decimal(output, output_size, position, value->tm_min, 2, '0') != 0 ||
                neon_append_char(output, output_size, position, ':') != 0 ||
                neon_append_decimal(output, output_size, position, value->tm_sec, 2, '0') != 0 ||
                neon_append_char(output, output_size, position, ' ') != 0) {
                return -1;
            }
            return neon_append_decimal(output, output_size, position, year, 4, '0');
        default:
            /* Lua validates format tokens before calling strftime. */
            return neon_append_char(output, output_size, position, specifier);
    }
}

void neon_time_set_epoch(time_t seconds_since_1970) {
    neon_timer_ensure_ready();

    neon_epoch_seconds = seconds_since_1970;
    neon_timer_epoch_counter = neon_timer_read_counter();
}

void neon_time_advance_seconds(time_t seconds) {
    neon_timer_ensure_ready();

    /*
        This remains useful for a future RTC synchronizer or manual time-set
        command. It adjusts the epoch base without changing monotonic clock().
    */
    neon_epoch_seconds += seconds;
}

clock_t clock(void) {
    neon_timer_ensure_ready();

    return neon_timer_ticks_to_clock(
        neon_timer_elapsed_ticks(neon_timer_boot_counter)
    );
}

time_t time(time_t* output) {
    time_t current;

    neon_timer_ensure_ready();

    current = neon_epoch_seconds +
        neon_timer_elapsed_seconds(neon_timer_epoch_counter);

    if (output != NULL) {
        *output = current;
    }

    return current;
}

double difftime(time_t end, time_t beginning) {
    return (double)(end - beginning);
}

struct tm* gmtime(const time_t* value) {
    time_t current = value == NULL ? time(NULL) : *value;

    return neon_break_time(current);
}

struct tm* localtime(const time_t* value) {
    /*
        NeonOS currently has no timezone database. Local time equals UTC
        until a timezone layer is added.
    */
    return gmtime(value);
}

time_t mktime(struct tm* value) {
    int year;
    int month;
    int day;
    long long days;
    time_t seconds;

    if (value == NULL) {
        return (time_t)-1;
    }

    year = value->tm_year + 1900;
    month = value->tm_mon;

    while (month < 0) {
        month += 12;
        year--;
    }

    while (month >= 12) {
        month -= 12;
        year++;
    }

    day = value->tm_mday;
    if (day <= 0) {
        day = 1;
    }

    days = neon_days_from_civil(year, (unsigned)month + 1U, (unsigned)day);
    seconds = (time_t)(
        days * SECONDS_PER_DAY +
        (long long)value->tm_hour * 3600LL +
        (long long)value->tm_min * 60LL +
        (long long)value->tm_sec
    );

    *value = *neon_break_time(seconds);
    return seconds;
}

size_t strftime(
    char* output,
    size_t output_size,
    const char* format,
    const struct tm* time_info
) {
    size_t position = 0;

    if (
        output == NULL ||
        output_size == 0 ||
        format == NULL ||
        time_info == NULL
    ) {
        return 0;
    }

    output[0] = '\0';

    while (*format != '\0') {
        if (*format != '%') {
            if (neon_append_char(output, output_size, &position, *format++) != 0) {
                return 0;
            }

            continue;
        }

        format++;

        if (*format == '\0') {
            return 0;
        }

        if (
            neon_append_strftime_item(
                output,
                output_size,
                &position,
                *format++,
                time_info
            ) != 0
        ) {
            return 0;
        }
    }

    return position;
}
