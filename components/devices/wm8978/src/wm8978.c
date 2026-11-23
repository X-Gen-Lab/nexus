/**
 * \file            wm8978.c
 * \brief           Settled9-bit codec writes and cooperative VMID sequencing
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/wm8978.h"

/** \brief A lost deadline domain cannot be repaired by a larger numeric time.
 */
static nx_result_t observe(nx_wm8978_t* device, nx_time_us_t deadline) {
    if (device->clock_failed)
        return NX_ERROR_STATE;
    nx_time_us_t now =
        device->config->clock.read(device->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        device->clock_failed = true;
        device->phase = NX_WM8978_UNCERTAIN;
        return NX_ERROR_STATE;
    }
    device->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief Synchronous endpoint return ends every stack descriptor/data borrow.
 */
static nx_result_t write_register(nx_wm8978_t* device, uint8_t address,
                                  uint16_t value, nx_time_us_t deadline) {
    nx_result_t result = observe(device, deadline);
    if (result != NX_SUCCESS) {
        device->phase = NX_WM8978_UNCERTAIN;
        return result;
    }
    const uint8_t bytes[2] = {
        (uint8_t)((uint16_t)((uint16_t)address << 1U) | (value >> 8U)),
        (uint8_t)value};
    nx_i2c_message_t message = nx_i2c_message_write(bytes, sizeof(bytes));
    size_t transferred = 0U;
    result = nx_i2c_endpoint_transaction(device->config->endpoint, &message, 1U,
                                         deadline, &transferred);
    if (result == NX_SUCCESS && transferred != sizeof(bytes))
        result = NX_ERROR_IO;
    nx_result_t after = observe(device, deadline);
    if (result == NX_SUCCESS)
        result = after;
    if (result != NX_SUCCESS)
        device->phase = NX_WM8978_UNCERTAIN;
    return result;
}

/** \brief Freeze all analogue outputs before manipulating bias or supplies. */
static nx_result_t mute_outputs(nx_wm8978_t* device, nx_time_us_t deadline) {
    nx_result_t result = write_register(device, 10U, 0x40U, deadline);
    for (uint8_t output = 52U; result == NX_SUCCESS && output <= 57U;
         ++output) {
        uint16_t mute = (output == 53U || output == 55U) ? 0x140U : 0x40U;
        result = write_register(device, output, mute, deadline);
    }
    return result;
}

/** \brief READY controls never reenter the nonreentrant physical owner. */
static nx_result_t admit(nx_wm8978_t* device, nx_time_us_t deadline) {
    if (device == NULL || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (device->in_call)
        return NX_ERROR_BUSY;
    if (device->phase != NX_WM8978_READY)
        return NX_ERROR_STATE;
    return NX_SUCCESS;
}

/** \brief LOUT1 holds a shadow; ROUT1 UPDATE publishes the stereo pair. */
static nx_result_t gains(nx_wm8978_t* device, int8_t left_db, int8_t right_db,
                         bool muted, nx_time_us_t deadline) {
    uint16_t left = (uint16_t)((int)left_db + 57);
    uint16_t right = (uint16_t)((int)right_db + 57);
    if (muted) {
        left |= 0x40U;
        right |= 0x40U;
    }
    nx_result_t result = write_register(device, 52U, left, deadline);
    return result == NX_SUCCESS
               ? write_register(device, 53U, (uint16_t)(right | 0x100U),
                                deadline)
               : result;
}

nx_result_t nx_wm8978_initialize(nx_wm8978_t* device,
                                 const nx_wm8978_config_t* config,
                                 nx_time_us_t deadline) {
    if (device == NULL || config == NULL || config->endpoint == NULL ||
        config->endpoint->ops == NULL ||
        config->endpoint->ops->transaction == NULL ||
        config->endpoint->context == NULL || config->clock.read == NULL ||
        config->vmid_settle_us == 0U ||
        config->maximum_clock_error_ppm > 1000U ||
        (config->word_bits != 16U && config->word_bits != 20U &&
         config->word_bits != 24U && config->word_bits != 32U) ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    const uint32_t nominal_mclk = 12288000U;
    uint32_t error = config->mclk_hz > nominal_mclk
                         ? config->mclk_hz - nominal_mclk
                         : nominal_mclk - config->mclk_hz;
    if ((uint64_t)error * UINT64_C(1000000) >
        (uint64_t)nominal_mclk * config->maximum_clock_error_ppm)
        return NX_ERROR_INVALID;
    if (device->in_call ||
        (device->phase != NX_WM8978_STOPPED &&
         device->phase != NX_WM8978_UNCERTAIN) ||
        (device->phase == NX_WM8978_UNCERTAIN && device->config != config))
        return NX_ERROR_BUSY;
    if (device->clock_failed)
        return NX_ERROR_STATE;
    nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER ||
        (device->config != NULL && now < device->last_clock)) {
        if (device->config != NULL) {
            device->clock_failed = true;
            device->phase = NX_WM8978_UNCERTAIN;
        }
        return NX_ERROR_STATE;
    }
    if (now >= deadline || deadline - now <= config->vmid_settle_us)
        return NX_ERROR_TIMEOUT;
    *device = (nx_wm8978_t){.config = config,
                            .deadline = deadline,
                            .last_clock = now,
                            .phase = NX_WM8978_UNCERTAIN,
                            .left_db = -57,
                            .right_db = -57,
                            .muted = true,
                            .in_call = true};
    nx_result_t result = write_register(device, 0U, 0U, deadline);
    if (result == NX_SUCCESS)
        result = mute_outputs(device, deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 3U, 0x0fU, deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 1U, 7U, deadline);
    if (result == NX_SUCCESS) {
        if (device->last_clock >= deadline ||
            deadline - device->last_clock <= config->vmid_settle_us) {
            result = NX_ERROR_TIMEOUT;
        } else {
            device->ready_at = device->last_clock + config->vmid_settle_us;
            device->phase = NX_WM8978_VMID_WAIT;
        }
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_wm8978_service(nx_wm8978_t* device) {
    if (device == NULL)
        return NX_ERROR_INVALID;
    if (device->in_call)
        return NX_ERROR_BUSY;
    if (device->phase == NX_WM8978_READY)
        return NX_SUCCESS;
    if (device->phase != NX_WM8978_VMID_WAIT)
        return NX_ERROR_STATE;
    nx_result_t result = observe(device, device->deadline);
    if (result != NX_SUCCESS) {
        device->phase = NX_WM8978_UNCERTAIN;
        return result;
    }
    if (device->last_clock < device->ready_at)
        return NX_ERROR_BUSY;
    device->in_call = true;
    const uint16_t format =
        (uint16_t)(0x10U | (device->config->word_bits == 16U   ? 0U
                            : device->config->word_bits == 20U ? 0x20U
                            : device->config->word_bits == 24U ? 0x40U
                                                               : 0x60U));
    result = write_register(device, 1U, 0x0fU, device->deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 2U, 0x180U, device->deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 4U, format, device->deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 6U, 0U, device->deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 7U, 0U, device->deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 50U, 1U, device->deadline);
    if (result == NX_SUCCESS)
        result = write_register(device, 51U, 1U, device->deadline);
    if (result == NX_SUCCESS)
        device->phase = NX_WM8978_READY;
    device->in_call = false;
    return result;
}

nx_result_t nx_wm8978_volume(nx_wm8978_t* device, int8_t left_db,
                             int8_t right_db, nx_time_us_t deadline) {
    if (left_db < -57 || left_db > 6 || right_db < -57 || right_db > 6)
        return NX_ERROR_INVALID;
    nx_result_t result = admit(device, deadline);
    if (result != NX_SUCCESS)
        return result;
    device->in_call = true;
    result = gains(device, left_db, right_db, device->muted, deadline);
    if (result == NX_SUCCESS) {
        device->left_db = left_db;
        device->right_db = right_db;
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_wm8978_mute(nx_wm8978_t* device, bool muted,
                           nx_time_us_t deadline) {
    nx_result_t result = admit(device, deadline);
    if (result != NX_SUCCESS)
        return result;
    device->in_call = true;
    if (muted)
        result = write_register(device, 10U, 0x40U, deadline);
    if (result == NX_SUCCESS)
        result =
            gains(device, device->left_db, device->right_db, muted, deadline);
    if (result == NX_SUCCESS && !muted)
        result = write_register(device, 10U, 0U, deadline);
    if (result == NX_SUCCESS)
        device->muted = muted;
    device->in_call = false;
    return result;
}

nx_result_t nx_wm8978_stop(nx_wm8978_t* device, nx_time_us_t deadline) {
    if (device == NULL || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (device->in_call)
        return NX_ERROR_BUSY;
    if (device->phase == NX_WM8978_STOPPED)
        return NX_SUCCESS;
    if (device->config == NULL)
        return NX_ERROR_STATE;
    device->in_call = true;
    nx_result_t result = mute_outputs(device, deadline);
    for (uint8_t power = 1U; result == NX_SUCCESS && power <= 3U; ++power)
        result = write_register(device, power, 0U, deadline);
    device->in_call = false;
    if (result == NX_SUCCESS)
        *device = (nx_wm8978_t){0};
    return result;
}
