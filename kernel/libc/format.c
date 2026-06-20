#include <stddef.h>
#include <stdint.h>

#include "stdarg.h"

#define FORMAT_ATTR __attribute__((noinline, used, optimize("O0")))

#define FORMAT_FIELD_CAPACITY 512
#define FORMAT_FLOAT_PRECISION_MAX 96

typedef struct FormatOutput {
    volatile char* buffer;
    size_t size;
    size_t written;
} FormatOutput;

typedef enum FormatLength {
    FORMAT_LENGTH_NONE,
    FORMAT_LENGTH_HH,
    FORMAT_LENGTH_H,
    FORMAT_LENGTH_L,
    FORMAT_LENGTH_LL,
    FORMAT_LENGTH_Z,
    FORMAT_LENGTH_T,
    FORMAT_LENGTH_J
} FormatLength;

typedef union FormatDoubleBits {
    double value;
    uint64_t bits;
} FormatDoubleBits;


static void format_put(FormatOutput* output, char character) {
    if (
        output->buffer != NULL &&
        output->size > 0 &&
        output->written < output->size - 1
    ) {
        output->buffer[output->written] = character;
    }

    output->written++;
}


static void format_put_repeat(
    FormatOutput* output,
    char character,
    size_t count
) {
    while (count != 0) {
        format_put(output, character);
        count--;
    }
}


static void format_put_text(
    FormatOutput* output,
    const char* text,
    size_t length
) {
    while (length != 0) {
        format_put(output, *text);
        text++;
        length--;
    }
}


static int format_is_digit(char character) {
    return character >= '0' && character <= '9';
}


static size_t format_text_length(const char* text) {
    size_t length = 0;

    while (text[length] != '\0') {
        length++;
    }

    return length;
}


static size_t format_unsigned_digits(
    char* output,
    unsigned long long value,
    unsigned int base,
    const char* digits
) {
    char reverse[64];
    size_t count = 0;
    size_t position = 0;

    if (value == 0) {
        output[0] = '0';
        return 1;
    }

    while (value != 0) {
        reverse[count] = digits[value % base];
        count++;
        value /= base;
    }

    while (count != 0) {
        count--;
        output[position] = reverse[count];
        position++;
    }

    return position;
}


static int format_double_is_nan(double value) {
    FormatDoubleBits number;

    number.value = value;

    return (
        (number.bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL &&
        (number.bits & 0x000FFFFFFFFFFFFFULL) != 0
    );
}


static int format_double_is_infinite(double value) {
    FormatDoubleBits number;

    number.value = value;

    return (number.bits & 0x7FFFFFFFFFFFFFFFULL) == 0x7FF0000000000000ULL;
}


static int format_double_is_negative(double value) {
    FormatDoubleBits number;

    number.value = value;

    return (number.bits & 0x8000000000000000ULL) != 0;
}


static double format_double_abs(double value) {
    FormatDoubleBits number;

    number.value = value;
    number.bits &= 0x7FFFFFFFFFFFFFFFULL;

    return number.value;
}


static void format_field_append_char(
    char* field,
    size_t* length,
    char character
) {
    if (*length + 1 < FORMAT_FIELD_CAPACITY) {
        field[*length] = character;
        *length += 1;
    }
}


static void format_field_append_text(
    char* field,
    size_t* length,
    const char* text,
    size_t text_length
) {
    while (text_length != 0) {
        format_field_append_char(field, length, *text);
        text++;
        text_length--;
    }
}


static void format_field_append_uint(
    char* field,
    size_t* length,
    unsigned long long value
) {
    char digits[64];
    size_t count;

    count = format_unsigned_digits(
        digits,
        value,
        10,
        "0123456789"
    );

    format_field_append_text(field, length, digits, count);
}


static void format_float_trim_g(
    char* field,
    size_t* length,
    int alternate
) {
    size_t exponent_position = *length;
    size_t decimal_position = *length;
    size_t position;

    if (alternate) {
        return;
    }

    for (position = 0; position < *length; position++) {
        if (field[position] == 'e' || field[position] == 'E') {
            exponent_position = position;
            break;
        }
    }

    for (position = 0; position < exponent_position; position++) {
        if (field[position] == '.') {
            decimal_position = position;
            break;
        }
    }

    if (decimal_position == exponent_position) {
        return;
    }

    position = exponent_position;

    while (position > decimal_position + 1 && field[position - 1] == '0') {
        position--;
    }

    if (position == decimal_position + 1) {
        position--;
    }

    if (exponent_position < *length) {
        size_t tail_length = *length - exponent_position;

        for (size_t i = 0; i < tail_length; i++) {
            field[position + i] = field[exponent_position + i];
        }

        *length = position + tail_length;
    } else {
        *length = position;
    }
}


static size_t format_float_fixed(
    char* field,
    double value,
    int precision,
    int alternate
) {
    unsigned long long whole;
    double fraction;
    char fraction_digits[FORMAT_FLOAT_PRECISION_MAX + 1];
    size_t length = 0;
    int carry = 0;

    /*
        A fixed decimal rendering for values above this range would need a
        bignum. Lua's own number formatting uses %g, so render extreme values
        in scientific notation instead.
    */
    if (value > 18446744073709549568.0) {
        return 0;
    }

    whole = (unsigned long long)value;
    fraction = value - (double)whole;

    for (int i = 0; i <= precision; i++) {
        int digit;

        fraction *= 10.0;
        digit = (int)fraction;

        if (digit < 0) {
            digit = 0;
        } else if (digit > 9) {
            digit = 9;
        }

        fraction_digits[i] = (char)digit;
        fraction -= (double)digit;
    }

    if (precision >= 0 && fraction_digits[precision] >= 5) {
        int index = precision - 1;

        carry = 1;

        while (index >= 0 && carry) {
            int digit = fraction_digits[index] + 1;

            if (digit == 10) {
                fraction_digits[index] = 0;
            } else {
                fraction_digits[index] = (char)digit;
                carry = 0;
            }

            index--;
        }

        if (carry) {
            whole++;
        }
    }

    format_field_append_uint(field, &length, whole);

    if (precision > 0 || alternate) {
        format_field_append_char(field, &length, '.');
    }

    for (int i = 0; i < precision; i++) {
        format_field_append_char(
            field,
            &length,
            (char)('0' + fraction_digits[i])
        );
    }

    return length;
}


static size_t format_float_scientific(
    char* field,
    double value,
    int precision,
    int alternate,
    char exponent_letter
) {
    char fraction_digits[FORMAT_FLOAT_PRECISION_MAX + 1];
    double normalized;
    int exponent = 0;
    int leading_digit;
    size_t length = 0;
    int carry = 0;

    if (value == 0.0) {
        normalized = 0.0;
    } else {
        normalized = value;

        while (normalized >= 10.0 && exponent < 400) {
            normalized *= 0.1;
            exponent++;
        }

        while (normalized < 1.0 && exponent > -400) {
            normalized *= 10.0;
            exponent--;
        }
    }

    leading_digit = (int)normalized;

    if (leading_digit < 0) {
        leading_digit = 0;
    } else if (leading_digit > 9) {
        leading_digit = 9;
    }

    normalized -= (double)leading_digit;

    for (int i = 0; i <= precision; i++) {
        int digit;

        normalized *= 10.0;
        digit = (int)normalized;

        if (digit < 0) {
            digit = 0;
        } else if (digit > 9) {
            digit = 9;
        }

        fraction_digits[i] = (char)digit;
        normalized -= (double)digit;
    }

    if (fraction_digits[precision] >= 5) {
        int index = precision - 1;

        carry = 1;

        while (index >= 0 && carry) {
            int digit = fraction_digits[index] + 1;

            if (digit == 10) {
                fraction_digits[index] = 0;
            } else {
                fraction_digits[index] = (char)digit;
                carry = 0;
            }

            index--;
        }

        if (carry) {
            leading_digit++;

            if (leading_digit == 10) {
                leading_digit = 1;
                exponent++;

                for (int i = 0; i < precision; i++) {
                    fraction_digits[i] = 0;
                }
            }
        }
    }

    format_field_append_char(field, &length, (char)('0' + leading_digit));

    if (precision > 0 || alternate) {
        format_field_append_char(field, &length, '.');
    }

    for (int i = 0; i < precision; i++) {
        format_field_append_char(
            field,
            &length,
            (char)('0' + fraction_digits[i])
        );
    }

    format_field_append_char(field, &length, exponent_letter);

    if (exponent < 0) {
        format_field_append_char(field, &length, '-');
        exponent = -exponent;
    } else {
        format_field_append_char(field, &length, '+');
    }

    if (exponent < 10) {
        format_field_append_char(field, &length, '0');
    }

    format_field_append_uint(field, &length, (unsigned int)exponent);

    return length;
}


static int format_float_decimal_exponent(double value) {
    int exponent = 0;

    if (value == 0.0) {
        return 0;
    }

    while (value >= 10.0 && exponent < 400) {
        value *= 0.1;
        exponent++;
    }

    while (value < 1.0 && exponent > -400) {
        value *= 10.0;
        exponent--;
    }

    return exponent;
}


static size_t format_float_value(
    char* field,
    double value,
    char specifier,
    int precision,
    int alternate
) {
    size_t length;
    int exponent;

    if (format_double_is_nan(value)) {
        field[0] = specifier >= 'A' && specifier <= 'Z' ? 'N' : 'n';
        field[1] = specifier >= 'A' && specifier <= 'Z' ? 'A' : 'a';
        field[2] = specifier >= 'A' && specifier <= 'Z' ? 'N' : 'n';
        return 3;
    }

    if (format_double_is_infinite(value)) {
        field[0] = specifier >= 'A' && specifier <= 'Z' ? 'I' : 'i';
        field[1] = specifier >= 'A' && specifier <= 'Z' ? 'N' : 'n';
        field[2] = specifier >= 'A' && specifier <= 'Z' ? 'F' : 'f';
        return 3;
    }

    if (precision < 0) {
        precision = 6;
    }

    if (precision > FORMAT_FLOAT_PRECISION_MAX) {
        precision = FORMAT_FLOAT_PRECISION_MAX;
    }

    if (specifier == 'f' || specifier == 'F') {
        length = format_float_fixed(field, value, precision, alternate);

        if (length != 0) {
            return length;
        }

        return format_float_scientific(
            field,
            value,
            precision,
            alternate,
            specifier == 'F' ? 'E' : 'e'
        );
    }

    if (specifier == 'e' || specifier == 'E') {
        return format_float_scientific(
            field,
            value,
            precision,
            alternate,
            specifier
        );
    }

    /*
        %g / %G: 'precision' means significant digits, not digits after the
        decimal point. Lua's default number conversion uses this form.
    */
    if (precision == 0) {
        precision = 1;
    }

    exponent = format_float_decimal_exponent(value);

    if (exponent < -4 || exponent >= precision) {
        length = format_float_scientific(
            field,
            value,
            precision - 1,
            alternate,
            specifier == 'G' ? 'E' : 'e'
        );
    } else {
        int fractional_digits = precision - (exponent + 1);

        if (fractional_digits < 0) {
            fractional_digits = 0;
        }

        length = format_float_fixed(
            field,
            value,
            fractional_digits,
            alternate
        );

        if (length == 0) {
            length = format_float_scientific(
                field,
                value,
                precision - 1,
                alternate,
                specifier == 'G' ? 'E' : 'e'
            );
        }
    }

    format_float_trim_g(field, &length, alternate);

    return length;
}


static void format_emit_field(
    FormatOutput* output,
    const char* field,
    size_t field_length,
    int width,
    int left_align,
    int zero_pad,
    size_t prefix_length
) {
    size_t padding = 0;

    if (width > 0 && (size_t)width > field_length) {
        padding = (size_t)width - field_length;
    }

    if (!left_align && !zero_pad) {
        format_put_repeat(output, ' ', padding);
    }

    if (!left_align && zero_pad && prefix_length != 0) {
        format_put_text(output, field, prefix_length);
        field += prefix_length;
        field_length -= prefix_length;
        prefix_length = 0;
    }

    if (!left_align && zero_pad) {
        format_put_repeat(output, '0', padding);
    }

    format_put_text(output, field, field_length);

    if (left_align) {
        format_put_repeat(output, ' ', padding);
    }
}


static unsigned long long format_get_unsigned(
    va_list* args,
    FormatLength length
) {
    switch (length) {
        case FORMAT_LENGTH_HH:
            return (unsigned char)va_arg(*args, unsigned int);

        case FORMAT_LENGTH_H:
            return (unsigned short)va_arg(*args, unsigned int);

        case FORMAT_LENGTH_L:
            return va_arg(*args, unsigned long);

        case FORMAT_LENGTH_LL:
            return va_arg(*args, unsigned long long);

        case FORMAT_LENGTH_Z:
            return va_arg(*args, size_t);

        case FORMAT_LENGTH_T:
            return (unsigned long long)va_arg(*args, ptrdiff_t);

        case FORMAT_LENGTH_J:
            return va_arg(*args, uintmax_t);

        case FORMAT_LENGTH_NONE:
        default:
            return va_arg(*args, unsigned int);
    }
}


static unsigned long long format_get_signed_magnitude(
    va_list* args,
    FormatLength length,
    int* negative
) {
    long long value;

    switch (length) {
        case FORMAT_LENGTH_HH:
            value = (signed char)va_arg(*args, int);
            break;

        case FORMAT_LENGTH_H:
            value = (short)va_arg(*args, int);
            break;

        case FORMAT_LENGTH_L:
            value = va_arg(*args, long);
            break;

        case FORMAT_LENGTH_LL:
            value = va_arg(*args, long long);
            break;

        case FORMAT_LENGTH_Z:
            value = (long long)va_arg(*args, ptrdiff_t);
            break;

        case FORMAT_LENGTH_T:
            value = (long long)va_arg(*args, ptrdiff_t);
            break;

        case FORMAT_LENGTH_J:
            value = (long long)va_arg(*args, intmax_t);
            break;

        case FORMAT_LENGTH_NONE:
        default:
            value = va_arg(*args, int);
            break;
    }

    if (value < 0) {
        *negative = 1;
        return (unsigned long long)(-(value + 1LL)) + 1ULL;
    }

    *negative = 0;
    return (unsigned long long)value;
}


static int format_vsnprintf_impl(
    char* buffer,
    size_t size,
    const char* format,
    va_list* args
) {
    FormatOutput output;

    output.buffer = (volatile char*)(void*)buffer;
    output.size = size;
    output.written = 0;

    if (format == NULL || args == NULL) {
        if (output.buffer != NULL && output.size > 0) {
            output.buffer[0] = '\0';
        }

        return 0;
    }

    while (*format != '\0') {
        int left_align = 0;
        int plus_sign = 0;
        int space_sign = 0;
        int alternate = 0;
        int zero_pad = 0;
        int width = 0;
        int precision = -1;
        FormatLength length = FORMAT_LENGTH_NONE;
        char specifier;

        if (*format != '%') {
            format_put(&output, *format);
            format++;
            continue;
        }

        format++;

        if (*format == '%') {
            format_put(&output, '%');
            format++;
            continue;
        }

        for (;;) {
            if (*format == '-') {
                left_align = 1;
            } else if (*format == '+') {
                plus_sign = 1;
            } else if (*format == ' ') {
                space_sign = 1;
            } else if (*format == '#') {
                alternate = 1;
            } else if (*format == '0') {
                zero_pad = 1;
            } else {
                break;
            }

            format++;
        }

        if (*format == '*') {
            width = va_arg(*args, int);

            if (width < 0) {
                left_align = 1;
                width = -width;
            }

            format++;
        } else {
            while (format_is_digit(*format)) {
                width = width * 10 + (*format - '0');
                format++;
            }
        }

        if (*format == '.') {
            format++;

            if (*format == '*') {
                precision = va_arg(*args, int);
                format++;
            } else {
                precision = 0;

                while (format_is_digit(*format)) {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }

            if (precision < 0) {
                precision = -1;
            }
        }

        if (*format == 'h') {
            format++;

            if (*format == 'h') {
                length = FORMAT_LENGTH_HH;
                format++;
            } else {
                length = FORMAT_LENGTH_H;
            }
        } else if (*format == 'l') {
            format++;

            if (*format == 'l') {
                length = FORMAT_LENGTH_LL;
                format++;
            } else {
                length = FORMAT_LENGTH_L;
            }
        } else if (*format == 'z') {
            length = FORMAT_LENGTH_Z;
            format++;
        } else if (*format == 't') {
            length = FORMAT_LENGTH_T;
            format++;
        } else if (*format == 'j') {
            length = FORMAT_LENGTH_J;
            format++;
        } else if (*format == 'L') {
            /*
                NeonOS currently uses double for Lua numbers. Accepting 'L'
                keeps the parser aligned, but reads double below.
            */
            length = FORMAT_LENGTH_L;
            format++;
        }

        specifier = *format;

        if (specifier == '\0') {
            format_put(&output, '%');
            break;
        }

        format++;

        if (specifier == 'c') {
            char field[1];
            int character = va_arg(*args, int);

            field[0] = (char)character;

            format_emit_field(
                &output,
                field,
                1,
                width,
                left_align,
                0,
                0
            );

            continue;
        }

        if (specifier == 's') {
            const char* text = va_arg(*args, const char*);
            size_t text_length;

            if (text == NULL) {
                text = "(null)";
            }

            text_length = format_text_length(text);

            if (precision >= 0 && text_length > (size_t)precision) {
                text_length = (size_t)precision;
            }

            format_emit_field(
                &output,
                text,
                text_length,
                width,
                left_align,
                0,
                0
            );

            continue;
        }

        if (
            specifier == 'd' ||
            specifier == 'i' ||
            specifier == 'u' ||
            specifier == 'o' ||
            specifier == 'x' ||
            specifier == 'X' ||
            specifier == 'p'
        ) {
            char field[128];
            char digits[64];
            const char* digit_set = "0123456789abcdef";
            unsigned long long value;
            unsigned int base = 10;
            size_t digits_length;
            size_t field_length = 0;
            size_t prefix_length = 0;
            int negative = 0;
            int signed_format =
                specifier == 'd' || specifier == 'i';
            int numeric_zero_pad;

            if (specifier == 'd' || specifier == 'i') {
                value = format_get_signed_magnitude(
                    args,
                    length,
                    &negative
                );
            } else if (specifier == 'p') {
                value = (unsigned long long)(uintptr_t)va_arg(*args, void*);
                base = 16;
            } else {
                value = format_get_unsigned(args, length);

                if (specifier == 'o') {
                    base = 8;
                } else if (specifier == 'x' || specifier == 'X') {
                    base = 16;

                    if (specifier == 'X') {
                        digit_set = "0123456789ABCDEF";
                    }
                }
            }

            digits_length = format_unsigned_digits(
                digits,
                value,
                base,
                digit_set
            );

            if (precision == 0 && value == 0 && specifier != 'p') {
                digits_length = 0;
            }

            if (signed_format) {
                if (negative) {
                    format_field_append_char(field, &field_length, '-');
                    prefix_length = 1;
                } else if (plus_sign) {
                    format_field_append_char(field, &field_length, '+');
                    prefix_length = 1;
                } else if (space_sign) {
                    format_field_append_char(field, &field_length, ' ');
                    prefix_length = 1;
                }
            }

            if (specifier == 'p') {
                format_field_append_char(field, &field_length, '0');
                format_field_append_char(field, &field_length, 'x');
                prefix_length += 2;
            } else if (
                alternate &&
                value != 0 &&
                (specifier == 'x' || specifier == 'X')
            ) {
                format_field_append_char(field, &field_length, '0');
                format_field_append_char(field, &field_length, specifier);
                prefix_length += 2;
            } else if (
                alternate &&
                value != 0 &&
                specifier == 'o' &&
                (precision < 0 || (size_t)precision <= digits_length)
            ) {
                format_field_append_char(field, &field_length, '0');
                prefix_length += 1;
            }

            while (
                precision >= 0 &&
                digits_length < (size_t)precision &&
                field_length + 1 < sizeof(field)
            ) {
                format_field_append_char(field, &field_length, '0');
                precision--;
            }

            format_field_append_text(
                field,
                &field_length,
                digits,
                digits_length
            );

            numeric_zero_pad =
                zero_pad &&
                !left_align &&
                precision < 0;

            format_emit_field(
                &output,
                field,
                field_length,
                width,
                left_align,
                numeric_zero_pad,
                prefix_length
            );

            continue;
        }

        if (
            specifier == 'f' ||
            specifier == 'F' ||
            specifier == 'e' ||
            specifier == 'E' ||
            specifier == 'g' ||
            specifier == 'G'
        ) {
            char field[FORMAT_FIELD_CAPACITY];
            double value = va_arg(*args, double);
            double magnitude = format_double_abs(value);
            size_t field_length = 0;
            size_t prefix_length = 0;
            int numeric_zero_pad;

            if (format_double_is_negative(value)) {
                format_field_append_char(field, &field_length, '-');
                prefix_length = 1;
            } else if (plus_sign) {
                format_field_append_char(field, &field_length, '+');
                prefix_length = 1;
            } else if (space_sign) {
                format_field_append_char(field, &field_length, ' ');
                prefix_length = 1;
            }

            field_length += format_float_value(
                field + field_length,
                magnitude,
                specifier,
                precision,
                alternate
            );

            numeric_zero_pad = zero_pad && !left_align;

            format_emit_field(
                &output,
                field,
                field_length,
                width,
                left_align,
                numeric_zero_pad,
                prefix_length
            );

            continue;
        }

        /*
            Preserve unknown specifiers visibly instead of consuming an
            argument incorrectly.
        */
        format_put(&output, '%');
        format_put(&output, specifier);
    }

    if (output.buffer != NULL && output.size > 0) {
        if (output.written < output.size) {
            output.buffer[output.written] = '\0';
        } else {
            output.buffer[output.size - 1] = '\0';
        }
    }

    return (int)output.written;
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
