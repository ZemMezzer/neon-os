#include <stddef.h>
#include <stdint.h>

#include "stdarg.h"

#define FORMAT_ATTR __attribute__((noinline, used, optimize("O0")))

static int format_vsnprintf_impl(
    char* buffer,
    size_t size,
    const char* format,
    va_list* args
) {
    volatile char* output = (volatile char*)(void*)buffer;
    size_t written = 0;

    #define FORMAT_PUT(character)                                    \
        do {                                                         \
            if (output && size > 0 && written < size - 1) {         \
                output[written] = (char)(character);                \
            }                                                        \
            written++;                                               \
        } while (0)

    if (!format) {
        if (output && size > 0) {
            output[0] = '\0';
        }

        return 0;
    }

    while (*format != '\0') {
        char specifier;

        if (*format != '%') {
            FORMAT_PUT(*format);
            format++;
            continue;
        }

        format++;
        specifier = *format;

        if (specifier == '\0') {
            FORMAT_PUT('%');
            break;
        }

        format++;

        if (specifier == '%') {
            FORMAT_PUT('%');
            continue;
        }

        if (specifier == 'c') {
            FORMAT_PUT(va_arg(*args, int));
            continue;
        }

        if (specifier == 's') {
            const char* text = va_arg(*args, const char*);

            if (!text) {
                text = "(null)";
            }

            while (*text != '\0') {
                FORMAT_PUT(*text);
                text++;
            }

            continue;
        }

        if (
            specifier == 'd' ||
            specifier == 'i' ||
            specifier == 'u' ||
            specifier == 'x' ||
            specifier == 'X' ||
            specifier == 'p'
        ) {
            char reverse[32];
            const char* digits = "0123456789abcdef";
            unsigned long long value;
            unsigned int base = 10;
            size_t digit_count = 0;
            int negative = 0;

            if (specifier == 'd' || specifier == 'i') {
                long long signed_value = (long long)va_arg(*args, int);

                if (signed_value < 0) {
                    negative = 1;
                    value =
                        (unsigned long long)(-(signed_value + 1)) + 1ULL;
                } else {
                    value = (unsigned long long)signed_value;
                }
            } else if (specifier == 'u') {
                value = (unsigned long long)va_arg(*args, unsigned int);
            } else if (specifier == 'p') {
                value =
                    (unsigned long long)(uintptr_t)va_arg(*args, void*);

                base = 16;

                FORMAT_PUT('0');
                FORMAT_PUT('x');
            } else {
                value = (unsigned long long)va_arg(*args, unsigned int);
                base = 16;

                if (specifier == 'X') {
                    digits = "0123456789ABCDEF";
                }
            }

            if (negative) {
                FORMAT_PUT('-');
            }

            if (value == 0) {
                FORMAT_PUT('0');
                continue;
            }

            while (value != 0) {
                reverse[digit_count] = digits[value % base];
                digit_count++;
                value /= base;
            }

            while (digit_count > 0) {
                digit_count--;
                FORMAT_PUT(reverse[digit_count]);
            }

            continue;
        }

        FORMAT_PUT('%');
        FORMAT_PUT(specifier);
    }

    if (output && size > 0) {
        if (written < size) {
            output[written] = '\0';
        } else {
            output[size - 1] = '\0';
        }
    }

    #undef FORMAT_PUT

    return (int)written;
}

int neon_vsnprintf_ptr(
    char* buffer,
    size_t size,
    const char* format,
    va_list* args
) FORMAT_ATTR;

int neon_vsnprintf_ptr(
    char* buffer,
    size_t size,
    const char* format,
    va_list* args
) {
    if (!args) {
        if (buffer && size > 0) {
            buffer[0] = '\0';
        }

        return 0;
    }

    return format_vsnprintf_impl(buffer, size, format, args);
}

int neon_snprintf(
    char* buffer,
    size_t size,
    const char* format,
    ...
) FORMAT_ATTR;

int neon_snprintf(
    char* buffer,
    size_t size,
    const char* format,
    ...
) {
    int result;
    va_list args;

    va_start(args, format);

    result = format_vsnprintf_impl(
        buffer,
        size,
        format,
        &args
    );

    va_end(args);

    return result;
}