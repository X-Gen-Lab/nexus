/**
 * \file            endian.c
 * \brief           Caller-owned endian algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/endian.h"

static bool valid_width(size_t width) {
    return width == 1 || width == 2 || width == 4 || width == 8;
}

nx_result_t nx_endian_read(const void* data, size_t width, bool little_endian,
                           uint64_t* out) {
    if (data == NULL || out == NULL || !valid_width(width)) {
        return NX_ERROR_INVALID;
    }
    const uint8_t* bytes = data;
    uint64_t value = 0;
    for (size_t index = 0; index < width; ++index) {
        const size_t offset = little_endian ? width - index - 1U : index;
        value = (value << 8U) | bytes[offset];
    }
    *out = value;
    return NX_SUCCESS;
}

nx_result_t nx_endian_write(void* data, size_t width, bool little_endian,
                            uint64_t value) {
    if (data == NULL || !valid_width(width)) {
        return NX_ERROR_INVALID;
    }
    if (width < 8 && value >= (UINT64_C(1) << (width * 8U))) {
        return NX_ERROR_OVERFLOW;
    }
    uint8_t* bytes = data;
    for (size_t index = 0; index < width; ++index) {
        const size_t offset = little_endian ? index : width - index - 1U;
        bytes[offset] = (uint8_t)(value & 0xFFU);
        value >>= 8U;
    }
    return NX_SUCCESS;
}
