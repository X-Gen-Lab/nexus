/**
 * \file            adc_format.c
 * \brief           Bounded integer scan decoding with explicit representation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_format.h"

/** \brief Validate alignment and the exclusive end before reading a span. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    return pointer != NULL && bytes != 0U && start % alignment == 0U &&
           bytes <= UINTPTR_MAX - start;
}

/** \brief Both exclusive ends have already passed checked span admission. */
static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return a < b + second_bytes && b < a + first_bytes;
}

/** \brief A validated cold layout bounds all later offset arithmetic. */
nx_result_t nx_adc_format_validate(const nx_adc_format_t* format) {
    if (!span(format, sizeof(*format), _Alignof(nx_adc_format_t))) {
        return NX_ERROR_INVALID;
    }
    if (format->storage_bytes == 0U || format->storage_bytes > 4U ||
        format->significant_bits == 0U || format->significant_bits > 32U ||
        (uint32_t)format->significant_bits + format->lsb_bits >
            (uint32_t)format->storage_bytes * 8U ||
        format->channel_count == 0U ||
        format->sample_stride < format->storage_bytes ||
        (format->encoding != NX_ADC_UNSIGNED &&
         format->encoding != NX_ADC_TWOS_COMPLEMENT) ||
        (format->byte_order != NX_ADC_LITTLE_ENDIAN &&
         format->byte_order != NX_ADC_BIG_ENDIAN) ||
        (format->group != NX_ADC_REGULAR && format->group != NX_ADC_INJECTED) ||
        (format->trigger != NX_ADC_TRIGGER_SOFTWARE &&
         format->trigger != NX_ADC_TRIGGER_TIMER &&
         format->trigger != NX_ADC_TRIGGER_EVENT) ||
        ((format->trigger == NX_ADC_TRIGGER_SOFTWARE) !=
         (format->trigger_identity == 0U))) {
        return NX_ERROR_INVALID;
    }
    size_t preceding = (size_t)format->channel_count - 1U;
    if (preceding >
            (SIZE_MAX - format->storage_bytes) / format->sample_stride ||
        format->scan_stride <
            preceding * format->sample_stride + format->storage_bytes) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief Reject all output aliases before the first sample byte is loaded. */
nx_result_t nx_adc_format_decode(const nx_adc_format_t* format,
                                 const void* samples, size_t bytes, size_t scan,
                                 size_t channel, int64_t* value) {
    if (nx_adc_format_validate(format) != NX_SUCCESS ||
        !span(samples, bytes, 1U) ||
        !span(value, sizeof(*value), _Alignof(int64_t)) ||
        bytes % format->scan_stride != 0U ||
        scan >= bytes / format->scan_stride ||
        channel >= format->channel_count ||
        overlap(format, sizeof(*format), samples, bytes) ||
        overlap(format, sizeof(*format), value, sizeof(*value)) ||
        overlap(samples, bytes, value, sizeof(*value))) {
        return NX_ERROR_INVALID;
    }
    /* Complete-scan bounds and the validated final channel bound prove both
     * products, their sum and every following byte lie inside samples. */
    const uint8_t* source = (const uint8_t*)samples +
                            scan * format->scan_stride +
                            channel * format->sample_stride;
    uint32_t raw = 0U;
    for (uint8_t index = 0U; index < format->storage_bytes; ++index) {
        uint32_t byte = source[index];
        uint32_t shift =
            format->byte_order == NX_ADC_LITTLE_ENDIAN
                ? (uint32_t)index * 8U
                : ((uint32_t)format->storage_bytes - 1U - index) * 8U;
        raw |= byte << shift;
    }
    raw >>= format->lsb_bits;
    if (format->significant_bits != 32U) {
        raw &= (UINT32_C(1) << format->significant_bits) - 1U;
    }
    int64_t decoded = raw;
    if (format->encoding == NX_ADC_TWOS_COMPLEMENT &&
        (raw & (UINT32_C(1) << (format->significant_bits - 1U))) != 0U) {
        decoded -= INT64_C(1) << format->significant_bits;
    }
    *value = decoded;
    return NX_SUCCESS;
}
