/**
 * \file            xpt2046.c
 * \brief           Settled differential XPT2046 conversion sequences
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/xpt2046.h"

#define XPT2046_X             0xd0U
#define XPT2046_Y             0x90U
#define XPT2046_Z1            0xb0U
#define XPT2046_Z2            0xc0U
#define XPT2046_TRANSFER      3U
#define XPT2046_CONVERSION_US UINT64_C(1600)

/** \brief Validate finite aligned storage before typed access or address sums.
 */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Known state/config/face extents cannot overlap copied output. */
static bool overlap(const void* a, size_t a_bytes, const void* b,
                    size_t b_bytes) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return first < second + b_bytes && second < first + a_bytes;
}

/** \brief Validate one exact live pin predicate, never an edge-queue fallback.
 */
static nx_result_t validate_config(const nx_xpt2046_t* device,
                                   const nx_xpt2046_config_t* config) {
    if (!span(config, sizeof(*config), _Alignof(nx_xpt2046_config_t)) ||
        overlap(device, sizeof(*device), config, sizeof(*config)) ||
        !span(config->spi, sizeof(*config->spi), _Alignof(nx_spi_endpoint_t)) ||
        config->spi->ops == NULL || config->spi->context == NULL ||
        config->spi->ops->transfer == NULL || config->clock.read == NULL ||
        config->clock_domain == 0U || config->clock_epoch == 0U ||
        overlap(device, sizeof(*device), config->spi, sizeof(*config->spi)) ||
        overlap(config, sizeof(*config), config->spi, sizeof(*config->spi)) ||
        ((config->pen != NULL) == (config->pen_exti != NULL))) {
        return NX_ERROR_INVALID;
    }
    if (config->pen != NULL) {
        if (!span(config->pen, sizeof(*config->pen),
                  _Alignof(nx_gpio_port_t)) ||
            config->pen->ops == NULL || config->pen->context == NULL ||
            config->pen->ops->read == NULL || config->pen_mask == 0U ||
            (config->pen_mask & (config->pen_mask - 1U)) != 0U ||
            overlap(device, sizeof(*device), config->pen,
                    sizeof(*config->pen)) ||
            overlap(config, sizeof(*config), config->pen,
                    sizeof(*config->pen)) ||
            overlap(config->spi, sizeof(*config->spi), config->pen,
                    sizeof(*config->pen))) {
            return NX_ERROR_INVALID;
        }
    } else {
        if (!span(config->pen_exti, sizeof(*config->pen_exti),
                  _Alignof(nx_exti_port_t)) ||
            config->pen_mask != 0U || config->pen_exti->ops == NULL ||
            config->pen_exti->context == NULL ||
            overlap(device, sizeof(*device), config->pen_exti,
                    sizeof(*config->pen_exti)) ||
            overlap(config, sizeof(*config), config->pen_exti,
                    sizeof(*config->pen_exti)) ||
            overlap(config->spi, sizeof(*config->spi), config->pen_exti,
                    sizeof(*config->pen_exti))) {
            return NX_ERROR_INVALID;
        }
        if (config->pen_exti->ops->level == NULL) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    return NX_SUCCESS;
}

/** \brief Reject exhausted, discontinuous or expired declared clock domains. */
static nx_result_t observe(nx_xpt2046_t* device, nx_time_us_t deadline) {
    const nx_xpt2046_config_t* config = device->config;
    nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        return NX_ERROR_IO;
    }
    device->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief One nonpipelined differential conversion always includes PD=00. */
static nx_result_t convert(nx_xpt2046_t* device, uint8_t command,
                           uint16_t* value, nx_time_us_t deadline) {
    nx_result_t result = observe(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_time_us_t conversion_deadline;
    result = nx_deadline_finite_after(device->last_clock, XPT2046_CONVERSION_US,
                                      &conversion_deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (deadline < conversion_deadline) {
        conversion_deadline = deadline;
    }
    const uint8_t tx[XPT2046_TRANSFER] = {command, 0U, 0U};
    uint8_t rx[XPT2046_TRANSFER] = {0U};
    size_t transferred = 0U;
    result = nx_spi_endpoint_transfer(device->config->spi, tx, rx, sizeof(tx),
                                      conversion_deadline, &transferred);
    nx_result_t timing = observe(device, conversion_deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (timing != NX_SUCCESS) {
        return timing;
    }
    if (transferred != sizeof(tx) || (rx[1] & 0x80U) != 0U ||
        (rx[2] & 7U) != 0U) {
        return NX_ERROR_IO;
    }
    *value = (uint16_t)((((uint16_t)rx[1] << 8U) | rx[2]) >> 3U) & 0x0fffU;
    return NX_SUCCESS;
}

/** \brief Lost publication permission never substitutes for converter shutdown.
 */
static nx_result_t uncertain(nx_xpt2046_t* device, nx_result_t result) {
    device->initialized = false;
    device->uncertain = true;
    return result;
}

nx_result_t nx_xpt2046_init(nx_xpt2046_t* device,
                            const nx_xpt2046_config_t* config,
                            nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_xpt2046_t))) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call || device->initialized ||
        (device->config != NULL && device->config != config)) {
        return NX_ERROR_BUSY;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    nx_result_t validation = validate_config(device, config);
    if (validation != NX_SUCCESS) {
        return validation;
    }
    device->in_call = true;
    nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER ||
        (device->config != NULL && now < device->last_clock)) {
        device->in_call = false;
        return NX_ERROR_IO;
    }
    if (now >= deadline) {
        device->in_call = false;
        return NX_ERROR_TIMEOUT;
    }
    device->config = config;
    device->last_clock = now;
    uint16_t ignored;
    nx_result_t result = convert(device, XPT2046_Y, &ignored, deadline);
    device->initialized = result == NX_SUCCESS;
    device->uncertain = result != NX_SUCCESS;
    device->in_call = false;
    return result;
}

/** \brief Observe the selected active-low level without consuming IRQ events.
 */
static nx_result_t pen_down(nx_xpt2046_t* device, bool* down) {
    const nx_xpt2046_config_t* config = device->config;
    if (config->pen_exti != NULL) {
        bool high = true;
        nx_result_t result = nx_exti_port_level(config->pen_exti, &high);
        if (result == NX_SUCCESS) {
            *down = !high;
        }
        return result;
    }
    uint32_t level = 0U;
    nx_result_t result = nx_gpio_port_read(config->pen, &level);
    if (result == NX_SUCCESS) {
        *down = (level & config->pen_mask) == 0U;
    }
    return result;
}

/** \brief Output cannot overwrite any known state, config or public face. */
static bool output_valid(const nx_xpt2046_t* device,
                         const nx_xpt2046_sample_t* sample) {
    const nx_xpt2046_config_t* config = device->config;
    return span(sample, sizeof(*sample), _Alignof(nx_xpt2046_sample_t)) &&
           !overlap(sample, sizeof(*sample), device, sizeof(*device)) &&
           !overlap(sample, sizeof(*sample), config, sizeof(*config)) &&
           !overlap(sample, sizeof(*sample), config->spi,
                    sizeof(*config->spi)) &&
           (config->pen != NULL
                ? !overlap(sample, sizeof(*sample), config->pen,
                           sizeof(*config->pen))
                : !overlap(sample, sizeof(*sample), config->pen_exti,
                           sizeof(*config->pen_exti)));
}

nx_result_t nx_xpt2046_sample(nx_xpt2046_t* device, nx_xpt2046_sample_t* sample,
                              nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_xpt2046_t))) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized || device->uncertain || device->config == NULL) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER || !output_valid(device, sample)) {
        return NX_ERROR_INVALID;
    }
    device->in_call = true;
    nx_result_t result = observe(device, deadline);
    bool down = false;
    if (result == NX_SUCCESS) {
        result = pen_down(device, &down);
    }
    if (result != NX_SUCCESS || !down) {
        /* No conversion was issued; timeout alone need not alter chip state. */
        if (result != NX_SUCCESS && result != NX_ERROR_TIMEOUT) {
            result = uncertain(device, result);
        }
        device->in_call = false;
        return result != NX_SUCCESS ? result : NX_ERROR_EMPTY;
    }
    nx_xpt2046_sample_t output = {0};
    result = convert(device, XPT2046_X, &output.x, deadline);
    if (result == NX_SUCCESS) {
        result = convert(device, XPT2046_Y, &output.y, deadline);
    }
    if (result == NX_SUCCESS && device->config->pressure) {
        result = convert(device, XPT2046_Z1, &output.z1, deadline);
        if (result == NX_SUCCESS) {
            result = convert(device, XPT2046_Z2, &output.z2, deadline);
        }
        output.pressure_valid = result == NX_SUCCESS;
    }
    if (result == NX_SUCCESS) {
        result = pen_down(device, &down);
    }
    if (result == NX_SUCCESS) {
        result = observe(device, deadline);
    }
    if (result != NX_SUCCESS) {
        result = uncertain(device, result);
    } else if (!down) {
        result = NX_ERROR_EMPTY;
    } else {
        output.observed =
            (nx_io_timestamp_t){.ticks = device->last_clock,
                                .ticks_per_second = 1000000U,
                                .domain = device->config->clock_domain,
                                .epoch = device->config->clock_epoch,
                                .source = NX_IO_TIMESTAMP_SERVICE_OBSERVATION};
        *sample = output;
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_xpt2046_stop(nx_xpt2046_t* device, nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_xpt2046_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call) {
        return NX_ERROR_BUSY;
    }
    if (device->config == NULL) {
        return NX_SUCCESS;
    }
    device->in_call = true;
    uint16_t ignored;
    nx_result_t result = convert(device, XPT2046_Y, &ignored, deadline);
    if (result == NX_SUCCESS) {
        device->config = NULL;
        device->last_clock = 0U;
        device->initialized = false;
        device->uncertain = false;
    } else {
        result = uncertain(device, result);
    }
    device->in_call = false;
    return result;
}
