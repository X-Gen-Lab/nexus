/**
 * \file            ili9341.c
 * \brief           Settled ILI9341 protocol and bounded cooperative startup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ili9341.h"

#define NX_ILI9341_RECOVERY_US UINT64_C(120000)
#define NX_ILI9341_STARTUP_US  UINT64_C(240000)

/** \brief Validate a complete aligned span before overlap arithmetic. */
static bool span_valid(const void* data, size_t bytes, size_t alignment) {
    uintptr_t first = (uintptr_t)data;
    return data != NULL && first % alignment == 0U &&
           first <= UINTPTR_MAX - bytes;
}

/** \brief Both input ranges have already passed span validation. */
static bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return first < second + bn && second < first + an;
}

/** \brief Payload and result storage cannot overwrite live immutable facts. */
static bool aliases_configuration(const nx_ili9341_t* device,
                                  const void* pointer, size_t bytes) {
    const nx_ili9341_config_t* config = device->config;
    return config != NULL &&
           (overlaps(pointer, bytes, config, sizeof(*config)) ||
            overlaps(pointer, bytes, config->endpoint,
                     sizeof(*config->endpoint)) ||
            overlaps(pointer, bytes, config->endpoint->ops,
                     sizeof(*config->endpoint->ops)) ||
            overlaps(pointer, bytes, config->endpoint->context, 1U) ||
            (config->clock.context != NULL &&
             overlaps(pointer, bytes, config->clock.context, 1U)));
}

/** \brief Every observation belongs to one live monotonic clock epoch. */
static nx_result_t observe(nx_ili9341_t* device, nx_time_us_t deadline,
                           nx_time_us_t* observed) {
    nx_time_us_t now =
        device->config->clock.read(device->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock)
        return NX_ERROR_STATE;
    device->last_clock = now;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (now >= deadline)
        return NX_ERROR_TIMEOUT;
    if (observed != NULL)
        *observed = now;
    return NX_SUCCESS;
}

/** \brief The first transport error has precedence over later time errors. */
static nx_result_t after_access(nx_ili9341_t* device, nx_result_t result,
                                nx_time_us_t deadline) {
    nx_result_t observed = observe(device, deadline, NULL);
    if (result == NX_SUCCESS)
        result = observed;
    if (result != NX_SUCCESS)
        device->phase = NX_ILI9341_UNCERTAIN;
    return result;
}

/** \brief Bound one settled command on both sides of its hardware access. */
static nx_result_t command(nx_ili9341_t* device, uint8_t value,
                           nx_time_us_t deadline) {
    nx_result_t result = observe(device, deadline, NULL);
    if (result == NX_SUCCESS)
        result = after_access(device,
                              nx_parallel16_endpoint_command(
                                  device->config->endpoint, value, deadline),
                              deadline);
    return result;
}

/** \brief Preserve the exact transferred prefix; never infer a missing word. */
static nx_result_t write_words(nx_ili9341_t* device, const uint16_t* words,
                               size_t count, size_t* transferred,
                               nx_time_us_t deadline) {
    *transferred = 0U;
    nx_result_t result = observe(device, deadline, NULL);
    if (result != NX_SUCCESS)
        return result;
    result = nx_parallel16_endpoint_write(device->config->endpoint, words,
                                          count, transferred, deadline);
    if (*transferred > count) {
        *transferred = 0U;
        result = NX_ERROR_IO;
    } else if (result == NX_SUCCESS && *transferred != count) {
        result = NX_ERROR_IO;
    }
    return after_access(device, result, deadline);
}

/** \brief Execute one command and low-byte parameter words. */
static nx_result_t parameters(nx_ili9341_t* device, uint8_t value,
                              const uint16_t* words, size_t count,
                              nx_time_us_t deadline) {
    nx_result_t result = command(device, value, deadline);
    size_t transferred;
    if (result == NX_SUCCESS)
        result = write_words(device, words, count, &transferred, deadline);
    return result;
}

/** \brief Recovery intervals are measured after the completed command. */
static nx_result_t arm_recovery(nx_ili9341_t* device,
                                nx_ili9341_phase_t phase) {
    nx_time_us_t now =
        device->config->clock.read(device->config->clock.context);
    nx_result_t result = NX_SUCCESS;
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        device->ready_at = NX_DEADLINE_NEVER;
        result = NX_ERROR_STATE;
    } else if (now >= NX_DEADLINE_NEVER - NX_ILI9341_RECOVERY_US) {
        device->last_clock = now;
        device->ready_at = NX_DEADLINE_NEVER;
        result = NX_ERROR_EXHAUSTED;
    } else {
        device->last_clock = now;
        device->ready_at = now + NX_ILI9341_RECOVERY_US;
        if (device->deadline <= device->ready_at)
            result = NX_ERROR_TIMEOUT;
    }
    if (result == NX_SUCCESS) {
        device->phase = phase;
    } else {
        device->phase = NX_ILI9341_UNCERTAIN;
    }
    return result;
}

nx_result_t nx_ili9341_initialize(nx_ili9341_t* device,
                                  const nx_ili9341_config_t* config,
                                  nx_time_us_t deadline) {
    if (!span_valid(device, sizeof(*device), _Alignof(nx_ili9341_t)) ||
        !span_valid(config, sizeof(*config), _Alignof(nx_ili9341_config_t)) ||
        overlaps(device, sizeof(*device), config, sizeof(*config)) ||
        !span_valid(config->endpoint, sizeof(*config->endpoint),
                    _Alignof(nx_parallel16_endpoint_t)) ||
        config->endpoint->ops == NULL || config->endpoint->context == NULL ||
        !span_valid(config->endpoint->context, 1U, 1U) ||
        (config->clock.context != NULL &&
         !span_valid(config->clock.context, 1U, 1U)) ||
        !span_valid(config->endpoint->ops, sizeof(*config->endpoint->ops),
                    _Alignof(nx_parallel16_ops_t)) ||
        overlaps(device, sizeof(*device), config->endpoint,
                 sizeof(*config->endpoint)) ||
        overlaps(device, sizeof(*device), config->endpoint->ops,
                 sizeof(*config->endpoint->ops)) ||
        overlaps(device, sizeof(*device), config->endpoint->context, 1U) ||
        (config->clock.context != NULL &&
         overlaps(device, sizeof(*device), config->clock.context, 1U)) ||
        config->endpoint->ops->command == NULL ||
        config->endpoint->ops->write == NULL ||
        config->endpoint->ops->read == NULL || config->clock.read == NULL ||
        (config->madctl & ~UINT8_C(0xe8)) != 0U ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (device->in_call || (device->phase != NX_ILI9341_STOPPED &&
                            device->phase != NX_ILI9341_UNCERTAIN))
        return NX_ERROR_BUSY;
    if (device->phase == NX_ILI9341_UNCERTAIN && device->config != config)
        return NX_ERROR_BUSY;
    device->in_call = true;
    nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER) {
        device->in_call = false;
        return NX_ERROR_STATE;
    }
    if (now >= deadline || deadline - now <= NX_ILI9341_STARTUP_US) {
        device->in_call = false;
        return NX_ERROR_TIMEOUT;
    }
    if (device->phase == NX_ILI9341_UNCERTAIN && now < device->last_clock) {
        device->in_call = false;
        return NX_ERROR_STATE;
    }
    if (device->phase == NX_ILI9341_UNCERTAIN && now < device->ready_at) {
        device->in_call = false;
        return NX_ERROR_BUSY;
    }
    device->config = config;
    device->last_clock = now;
    device->deadline = deadline;
    device->width = (config->madctl & UINT8_C(0x20)) != 0U ? 320U : 240U;
    device->height = (config->madctl & UINT8_C(0x20)) != 0U ? 240U : 320U;
    device->phase = NX_ILI9341_UNCERTAIN;
    device->in_call = true;
    nx_result_t result = command(device, UINT8_C(0xd3), deadline);
    uint16_t identity[4] = {0U, 0U, 0U, 0U};
    size_t transferred = 0U;
    if (result == NX_SUCCESS) {
        result = nx_parallel16_endpoint_read(device->config->endpoint, identity,
                                             4U, &transferred, deadline);
        if (transferred > 4U || (result == NX_SUCCESS && transferred != 4U))
            result = NX_ERROR_IO;
        result = after_access(device, result, deadline);
    }
    if (result == NX_SUCCESS &&
        ((identity[2] & UINT16_C(0xff)) != UINT16_C(0x93) ||
         (identity[3] & UINT16_C(0xff)) != UINT16_C(0x41)))
        result = NX_ERROR_UNSUPPORTED;
    if (result == NX_SUCCESS) {
        result = command(device, UINT8_C(0x01), deadline);
        nx_result_t recovered = arm_recovery(device, NX_ILI9341_RESET_WAIT);
        if (result == NX_SUCCESS)
            result = recovered;
    }
    if (result != NX_SUCCESS)
        device->phase = NX_ILI9341_UNCERTAIN;
    device->in_call = false;
    return result;
}

nx_result_t nx_ili9341_service(nx_ili9341_t* device) {
    if (device == NULL)
        return NX_ERROR_INVALID;
    if (device->in_call)
        return NX_ERROR_BUSY;
    if (device->phase == NX_ILI9341_READY)
        return NX_SUCCESS;
    if (device->phase != NX_ILI9341_RESET_WAIT &&
        device->phase != NX_ILI9341_SLEEP_WAIT)
        return NX_ERROR_STATE;
    device->in_call = true;
    nx_time_us_t now;
    nx_result_t result = observe(device, device->deadline, &now);
    if (result == NX_SUCCESS && now < device->ready_at) {
        device->in_call = false;
        return NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS && device->phase == NX_ILI9341_RESET_WAIT) {
        result = command(device, UINT8_C(0x11), device->deadline);
        nx_result_t recovered = arm_recovery(device, NX_ILI9341_SLEEP_WAIT);
        if (result == NX_SUCCESS)
            result = recovered;
        if (result != NX_SUCCESS)
            device->phase = NX_ILI9341_UNCERTAIN;
        device->in_call = false;
        return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
    }
    const uint16_t pixel_format = UINT16_C(0x55);
    const uint16_t orientation = device->config->madctl;
    if (result == NX_SUCCESS)
        result = parameters(device, UINT8_C(0x3a), &pixel_format, 1U,
                            device->deadline);
    if (result == NX_SUCCESS)
        result = parameters(device, UINT8_C(0x36), &orientation, 1U,
                            device->deadline);
    if (result == NX_SUCCESS)
        result = command(device, UINT8_C(0x29), device->deadline);
    device->phase =
        result == NX_SUCCESS ? NX_ILI9341_READY : NX_ILI9341_UNCERTAIN;
    device->in_call = false;
    return result;
}

nx_result_t nx_ili9341_write(nx_ili9341_t* device, uint16_t x, uint16_t y,
                             uint16_t width, uint16_t height,
                             const uint16_t* pixels, size_t count,
                             size_t* written, nx_time_us_t deadline) {
    if (device == NULL || width == 0U || height == 0U ||
        count != (size_t)width * height || count > 76800U ||
        !span_valid(pixels, count * sizeof(*pixels), _Alignof(uint16_t)) ||
        !span_valid(written, sizeof(*written), _Alignof(size_t)) ||
        overlaps(device, sizeof(*device), pixels, count * sizeof(*pixels)) ||
        overlaps(device, sizeof(*device), written, sizeof(*written)) ||
        overlaps(pixels, count * sizeof(*pixels), written, sizeof(*written)) ||
        aliases_configuration(device, pixels, count * sizeof(*pixels)) ||
        aliases_configuration(device, written, sizeof(*written)))
        return NX_ERROR_INVALID;
    if (device->in_call)
        return NX_ERROR_BUSY;
    if (device->phase != NX_ILI9341_READY)
        return NX_ERROR_STATE;
    if (x >= device->width || y >= device->height ||
        width > device->width - x || height > device->height - y)
        return NX_ERROR_INVALID;
    device->in_call = true;
    nx_result_t result = observe(device, deadline, NULL);
    if (result != NX_SUCCESS) {
        device->in_call = false;
        return result;
    }
    uint16_t x_end = (uint16_t)(x + width - 1U);
    uint16_t y_end = (uint16_t)(y + height - 1U);
    const uint16_t columns[4] = {(uint16_t)(x >> 8U), x & UINT16_C(0xff),
                                 (uint16_t)(x_end >> 8U),
                                 x_end & UINT16_C(0xff)};
    const uint16_t pages[4] = {(uint16_t)(y >> 8U), y & UINT16_C(0xff),
                               (uint16_t)(y_end >> 8U), y_end & UINT16_C(0xff)};
    *written = 0U;
    result = parameters(device, UINT8_C(0x2a), columns, 4U, deadline);
    if (result == NX_SUCCESS)
        result = parameters(device, UINT8_C(0x2b), pages, 4U, deadline);
    if (result == NX_SUCCESS)
        result = command(device, UINT8_C(0x2c), deadline);
    if (result == NX_SUCCESS)
        result = write_words(device, pixels, count, written, deadline);
    if (result != NX_SUCCESS)
        device->phase = NX_ILI9341_UNCERTAIN;
    device->in_call = false;
    return result;
}

nx_result_t nx_ili9341_stop(nx_ili9341_t* device, nx_time_us_t deadline) {
    if (device == NULL)
        return NX_ERROR_INVALID;
    if (device->in_call)
        return NX_ERROR_BUSY;
    if (device->phase == NX_ILI9341_STOPPED)
        return NX_SUCCESS;
    device->in_call = true;
    nx_result_t result = observe(device, deadline, NULL);
    if (result != NX_SUCCESS) {
        device->in_call = false;
        return result;
    }
    if (device->last_clock < device->ready_at) {
        device->in_call = false;
        return NX_ERROR_BUSY;
    }
    result = command(device, UINT8_C(0x28), deadline);
    if (result == NX_SUCCESS)
        *device = (nx_ili9341_t){0};
    else {
        device->phase = NX_ILI9341_UNCERTAIN;
        device->in_call = false;
    }
    return result;
}
