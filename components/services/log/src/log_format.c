/**
 * \file            log_format.c
 * \brief           Restricted C11 formatting before whole-record publication
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/log_format.h"
#include "log_format_impl.h"
#include <inttypes.h>
#include <string.h>

/** \brief Reject wrapped or overlapping caller scratch before rendering. */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Bound literal numeric formatting work without consuming arguments. */
static bool number(const char* format, size_t* cursor, size_t limit) {
    unsigned value = 0U;
    while (*cursor < limit && format[*cursor] >= '0' &&
           format[*cursor] <= '9') {
        value = value * 10U + (unsigned)(format[*cursor] - '0');
        if (value > 1024U) {
            return false;
        }
        ++*cursor;
    }
    return true;
}

/** \brief Admit only conversions with explicit finite input and output bounds.
 */
static bool admitted(const char* format, size_t* length) {
    size_t size = 0U;
    while (size < 256U && format[size] != '\0') {
        ++size;
    }
    if (size == 256U) {
        return false;
    }
    for (size_t i = 0U; i < size; ++i) {
        if (format[i] != '%') {
            continue;
        }
        ++i;
        if (i < size && format[i] == '%') {
            continue;
        }
        while (i < size && strchr("-+ #0", format[i]) != NULL) {
            ++i;
        }
        if (!number(format, &i, size)) {
            return false;
        }
        bool precision = false;
        if (i < size && format[i] == '.') {
            precision = true;
            ++i;
            if (!number(format, &i, size)) {
                return false;
            }
        }
        bool length_modifier = false;
        if (i < size && strchr("hljzt", format[i]) != NULL) {
            char modifier = format[i++];
            length_modifier = true;
            if (i < size && (modifier == 'h' || modifier == 'l') &&
                format[i] == modifier) {
                ++i;
            }
        }
        if (i == size) {
            return false;
        }
        if (strchr("diouxX", format[i]) != NULL) {
            continue;
        }
        if (length_modifier ||
            (format[i] != 'p' && format[i] != 'c' && format[i] != 's') ||
            (format[i] == 's' && !precision)) {
            return false;
        }
    }
    *length = size;
    return true;
}

/** \brief Admitted promoted integer lengths, without floating-point state. */
typedef enum {
    LENGTH_DEFAULT,
    LENGTH_HH,
    LENGTH_H,
    LENGTH_L,
    LENGTH_LL
} length_t;

/** \brief Literal width and precision are validated before argument access. */
typedef struct {
    bool left, zero, plus, space, alternate, has_precision;
    unsigned width, precision;
    length_t length;
} specification_t;

/** \brief Count output even when caller capacity prevents storing a suffix. */
typedef struct {
    char* bytes;
    size_t capacity, count;
} output_t;

/** \brief Every rendered byte is bounded by exclusive caller storage. */
static void put(output_t* out, char byte) {
    if (out->count < out->capacity - 1U)
        out->bytes[out->count] = byte;
    ++out->count;
}

/** \brief Literal admitted widths/precisions bound each padding loop. */
static void repeat(output_t* out, char byte, size_t count) {
    for (size_t i = 0U; i < count; ++i)
        put(out, byte);
}

/** \brief The initial validation pass already bounded decimal literals. */
static unsigned decimal(const char* format, size_t* cursor) {
    unsigned value = 0U;
    while (format[*cursor] >= '0' && format[*cursor] <= '9')
        value = value * 10U + (unsigned)(format[(*cursor)++] - '0');
    return value;
}

/** \brief Decode only the restricted, already-admitted format policy. */
static specification_t specification(const char* format, size_t* cursor) {
    specification_t spec = {0};
    for (;;) {
        char flag = format[*cursor];
        if (flag == '-')
            spec.left = true;
        else if (flag == '0')
            spec.zero = true;
        else if (flag == '+')
            spec.plus = true;
        else if (flag == ' ')
            spec.space = true;
        else if (flag == '#')
            spec.alternate = true;
        else
            break;
        ++*cursor;
    }
    spec.width = decimal(format, cursor);
    if (format[*cursor] == '.') {
        ++*cursor;
        spec.has_precision = true;
        spec.precision = decimal(format, cursor);
    }
    char modifier = format[*cursor];
    if (modifier == 'h' || modifier == 'l') {
        ++*cursor;
        bool twice = format[*cursor] == modifier;
        if (twice)
            ++*cursor;
        spec.length = modifier == 'h' ? (twice ? LENGTH_HH : LENGTH_H)
                                      : (twice ? LENGTH_LL : LENGTH_L);
    } else if (modifier == 'j' || modifier == 'z' || modifier == 't') {
        ++*cursor;
        if (modifier == 'j') {
            bool signed_conversion =
                format[*cursor] == 'd' || format[*cursor] == 'i';
            if (signed_conversion)
                spec.length = _Generic((intmax_t)0, int
                                       : LENGTH_DEFAULT, long
                                       : LENGTH_L, long long
                                       : LENGTH_LL);
            else
                spec.length = _Generic((uintmax_t)0, unsigned
                                       : LENGTH_DEFAULT, unsigned long
                                       : LENGTH_L, unsigned long long
                                       : LENGTH_LL);
        }
        if (modifier == 'z')
            spec.length = _Generic((size_t)0, unsigned
                                   : LENGTH_DEFAULT, unsigned long
                                   : LENGTH_L, unsigned long long
                                   : LENGTH_LL);
        if (modifier == 't')
            spec.length = _Generic((ptrdiff_t)0, int
                                   : LENGTH_DEFAULT, long
                                   : LENGTH_L, long long
                                   : LENGTH_LL);
    }
    return spec;
}

/** \brief Render sign/prefix before zero padding with bounded local digits. */
static void integer(output_t* out, specification_t spec, char type,
                    uintmax_t value, bool negative) {
    unsigned base = type == 'o'                                   ? 8U
                    : (type == 'x' || type == 'X' || type == 'p') ? 16U
                                                                  : 10U;
    const char* alphabet =
        type == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
    char digits[sizeof(uintmax_t) * 3U];
    size_t count = 0U;
    uintmax_t remaining = value;
    if (value != 0U || !spec.has_precision || spec.precision != 0U ||
        type == 'p') {
        do {
            digits[count++] = alphabet[(size_t)(remaining % base)];
            remaining /= base;
        } while (remaining != 0U);
    }
    size_t zeros = spec.has_precision && spec.precision > count
                       ? spec.precision - count
                       : 0U;
    if (base == 8U && spec.alternate && zeros == 0U &&
        (count == 0U || digits[count - 1U] != '0'))
        ++zeros;
    char sign = 0;
    if (type == 'd' || type == 'i')
        sign = (char)(negative ? '-' : spec.plus ? '+' : spec.space ? ' ' : 0);
    bool prefix = type == 'p' || (base == 16U && spec.alternate && value != 0U);
    size_t size = count + zeros + (sign != 0 ? 1U : 0U) + (prefix ? 2U : 0U);
    size_t padding = spec.width > size ? spec.width - size : 0U;
    if (spec.zero && !spec.left && !spec.has_precision)
        zeros += padding;
    else if (!spec.left)
        repeat(out, ' ', padding);
    if (sign != 0)
        put(out, sign);
    if (prefix) {
        put(out, '0');
        put(out, type == 'X' ? 'X' : 'x');
    }
    repeat(out, '0', zeros);
    while (count != 0U)
        put(out, digits[--count]);
    if (spec.left)
        repeat(out, ' ', padding);
}

/** \brief Precision limits all string reads; no character temporary is indexed.
 */
static bool text(output_t* out, specification_t spec, const char* bytes) {
    if (bytes == NULL)
        return false;
    size_t count = 0U;
    while (count < spec.precision && bytes[count] != '\0')
        ++count;
    size_t padding = spec.width > count ? spec.width - count : 0U;
    if (!spec.left)
        repeat(out, ' ', padding);
    for (size_t i = 0U; i < count; ++i)
        put(out, bytes[i]);
    if (spec.left)
        repeat(out, ' ', padding);
    return true;
}

/** \brief Shared renderer invokes no libc formatter, heap or transport. */
nx_result_t nx_log_render(char* buffer, size_t capacity, size_t* length,
                          const char* format, va_list arguments) {
    size_t format_length = 0U;
    if (buffer == NULL || capacity == 0U || length == NULL || format == NULL ||
        !admitted(format, &format_length) ||
        overlaps(buffer, capacity, format, format_length + 1U) ||
        overlaps(buffer, capacity, length, sizeof(*length)))
        return NX_ERROR_INVALID;
    output_t out = {buffer, capacity, 0U};
    va_list values;
    va_copy(values, arguments);
    bool valid = true;
    for (size_t i = 0U; i < format_length && valid; ++i) {
        if (format[i] != '%') {
            put(&out, format[i]);
            continue;
        }
        ++i;
        if (format[i] == '%') {
            put(&out, '%');
            continue;
        }
        specification_t spec = specification(format, &i);
        char type = format[i];
        if (type == 's')
            valid = text(&out, spec, va_arg(values, const char*));
        else if (type == 'c') {
            char byte = (char)va_arg(values, int);
            size_t padding = spec.width > 1U ? spec.width - 1U : 0U;
            if (!spec.left)
                repeat(&out, ' ', padding);
            put(&out, byte);
            if (spec.left)
                repeat(&out, ' ', padding);
        } else if (type == 'd' || type == 'i') {
            intmax_t value;
            switch (spec.length) {
                case LENGTH_HH:
                    value = (intmax_t)(signed char)va_arg(values, int);
                    break;
                case LENGTH_H:
                    value = (short)va_arg(values, int);
                    break;
                case LENGTH_L:
                    value = va_arg(values, long);
                    break;
                case LENGTH_LL:
                    value = (intmax_t)va_arg(values, long long);
                    break;
                default:
                    value = va_arg(values, int);
                    break;
            }
            bool negative = value < 0;
            uintmax_t magnitude =
                negative ? 0U - (uintmax_t)value : (uintmax_t)value;
            integer(&out, spec, type, magnitude, negative);
        } else {
            uintmax_t value;
            if (type == 'p')
                value = (uintmax_t)(uintptr_t)va_arg(values, void*);
            else {
                switch (spec.length) {
                    case LENGTH_HH:
                        value = (unsigned char)va_arg(values, unsigned);
                        break;
                    case LENGTH_H:
                        value = (unsigned short)va_arg(values, unsigned);
                        break;
                    case LENGTH_L:
                        value = va_arg(values, unsigned long);
                        break;
                    case LENGTH_LL:
                        value = (uintmax_t)va_arg(values, unsigned long long);
                        break;
                    default:
                        value = va_arg(values, unsigned);
                        break;
                }
            }
            integer(&out, spec, type, value, false);
        }
    }
    va_end(values);
    buffer[out.count < capacity ? out.count : capacity - 1U] = '\0';
    if (!valid)
        return NX_ERROR_INVALID;
    if (out.count >= capacity)
        return NX_ERROR_OVERFLOW;
    *length = out.count;
    return NX_SUCCESS;
}

/** \brief Publish only a complete record rendered in caller scratch. */
nx_result_t nx_log_vprintf(nx_log_t* logger, nx_log_level_t level, char* buffer,
                           size_t capacity, nx_time_us_t deadline,
                           const char* format, va_list arguments) {
    if (logger == NULL || (unsigned)level > (unsigned)NX_LOG_ERROR) {
        return NX_ERROR_INVALID;
    }
    if (level < logger->minimum) {
        return nx_log_write(logger, level, NULL, 0U, deadline);
    }
    if (overlaps(buffer, capacity, logger, sizeof(*logger))) {
        return NX_ERROR_INVALID;
    }
    size_t length = 0U;
    nx_result_t result =
        nx_log_render(buffer, capacity, &length, format, arguments);
    return result == NX_SUCCESS
               ? nx_log_write(logger, level, buffer, length, deadline)
               : result;
}

/** \brief Preserve the caller argument list lifecycle around one formatting. */
nx_result_t nx_log_printf(nx_log_t* logger, nx_log_level_t level, char* buffer,
                          size_t capacity, nx_time_us_t deadline,
                          const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    nx_result_t result = nx_log_vprintf(logger, level, buffer, capacity,
                                        deadline, format, arguments);
    va_end(arguments);
    return result;
}
