/**
 * \file            crc.c
 * \brief           Caller-owned crc algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/crc.h"

const nx_crc_model_t nx_crc32_iso_hdlc = {0x04C11DB7U, UINT32_MAX, UINT32_MAX,
                                          32,          true,       true};
const nx_crc_model_t nx_crc16_modbus = {0x8005U, 0xFFFFU, 0, 16, true, true};
const nx_crc_model_t nx_crc8_smbus = {0x07U, 0, 0, 8, false, false};

static uint32_t crc_mask(uint8_t width) {
    return width == 32 ? UINT32_MAX : (UINT32_C(1) << width) - 1U;
}

static bool valid_model(const nx_crc_model_t* model) {
    if (model == NULL || model->width < 8 || model->width > 32) {
        return false;
    }
    const uint32_t mask = crc_mask(model->width);
    return model->polynomial != 0 && (model->polynomial & ~mask) == 0 &&
           (model->initial & ~mask) == 0 && (model->xor_out & ~mask) == 0;
}

static uint32_t reflect_bits(uint32_t value, uint8_t count) {
    uint32_t reflected = 0;
    for (uint8_t index = 0; index < count; ++index) {
        reflected = (reflected << 1U) | (value & 1U);
        value >>= 1U;
    }
    return reflected;
}

nx_result_t nx_crc_update(const nx_crc_model_t* model, const void* data,
                          size_t length, uint32_t* state) {
    if (!valid_model(model) || state == NULL || (data == NULL && length != 0)) {
        return NX_ERROR_INVALID;
    }
    const uint32_t mask = crc_mask(model->width);
    if ((*state & ~mask) != 0) {
        return NX_ERROR_INVALID;
    }
    const uint8_t* bytes = data;
    const uint32_t high = UINT32_C(1) << (model->width - 1U);
    uint32_t current = *state;
    for (size_t index = 0; index < length; ++index) {
        const uint32_t byte =
            model->reflect_input ? reflect_bits(bytes[index], 8) : bytes[index];
        current ^= byte << (model->width - 8U);
        for (unsigned bit = 0; bit < 8; ++bit) {
            const bool carry = (current & high) != 0;
            current = (current << 1U) & mask;
            if (carry) {
                current ^= model->polynomial;
            }
        }
    }
    *state = current;
    return NX_SUCCESS;
}

nx_result_t nx_crc_finish(const nx_crc_model_t* model, uint32_t state,
                          uint32_t* out) {
    if (!valid_model(model) || out == NULL ||
        (state & ~crc_mask(model->width)) != 0) {
        return NX_ERROR_INVALID;
    }
    const uint32_t value =
        model->reflect_output ? reflect_bits(state, model->width) : state;
    *out = value ^ model->xor_out;
    return NX_SUCCESS;
}
