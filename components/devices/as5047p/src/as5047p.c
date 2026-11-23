/**
 * \file            as5047p.c
 * \brief           AS5047P checked two-frame reads and explicit error recovery
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/as5047p.h"
#include <string.h>

/** \brief Validate known typed spans before any control access. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Only known spans can be checked; opaque contexts stay caller-owned.
 */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return an > UINTPTR_MAX - first || bn > UINTPTR_MAX - second ||
           (first < second + bn && second < first + an);
}

/** \brief Fold the lower fifteen bits to the even-parity command bit. */
static uint16_t parity(uint16_t word) {
    uint16_t folded = word;
    folded ^= folded >> 8U;
    folded ^= folded >> 4U;
    folded ^= folded >> 2U;
    folded ^= folded >> 1U;
    return (uint16_t)(word | ((folded & 1U) << 15U));
}

/** \brief All frames consume the original monotonic finite operation budget. */
static nx_result_t observe(nx_as5047p_t* device, nx_time_us_t deadline) {
    nx_time_us_t now =
        device->config->clock.read(device->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        return NX_ERROR_STATE;
    }
    device->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief One endpoint transfer has exactly one complete sixteen-clock CS. */
static nx_result_t frame(nx_as5047p_t* device, uint16_t command,
                         uint16_t* response, nx_time_us_t deadline) {
    nx_result_t result = observe(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint8_t tx[2] = {(uint8_t)(command >> 8U), (uint8_t)command};
    uint8_t rx[2];
    size_t transferred = 0U;
    result = nx_spi_endpoint_transfer(device->config->spi, tx, rx, sizeof(tx),
                                      deadline, &transferred);
    nx_result_t timing = observe(device, deadline);
    if (result != NX_SUCCESS || timing != NX_SUCCESS) {
        return result != NX_SUCCESS ? result : timing;
    }
    if (transferred != sizeof(tx)) {
        return NX_ERROR_IO;
    }
    *response = (uint16_t)(((uint16_t)rx[0] << 8U) | rx[1]);
    return NX_SUCCESS;
}

/** \brief Discard the old pipeline reply and validate only this command's data.
 */
static nx_result_t read_register(nx_as5047p_t* device, uint16_t reg,
                                 bool allow_error, uint16_t* data,
                                 nx_time_us_t deadline) {
    uint16_t response;
    nx_result_t result =
        frame(device, parity((uint16_t)(0x4000U | reg)), &response, deadline);
    if (result == NX_SUCCESS) {
        result = frame(device, parity(0x4000U), &response, deadline);
    }
    if (result == NX_SUCCESS) {
        if (parity(response & 0x7fffU) != response ||
            (!allow_error && (response & 0x4000U) != 0U)) {
            return NX_ERROR_IO;
        }
        *data = response & 0x3fffU;
    }
    return result;
}

/** \brief Diagnostics remain separate from angle units and product limits. */
static nx_result_t diagnostics(nx_as5047p_t* device, uint16_t* value,
                               nx_time_us_t deadline) {
    nx_result_t result = read_register(device, 0x3ffcU, false, value, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((*value & 0x0200U) != 0U) {
        return NX_ERROR_IO;
    }
    return (*value & 0x0100U) == 0U ? NX_ERROR_BUSY : NX_SUCCESS;
}

/** \brief Validate a stable public endpoint without claiming its waveform. */
static bool binding(const nx_as5047p_t* device,
                    const nx_as5047p_config_t* config) {
    return span(config, sizeof(*config), _Alignof(nx_as5047p_config_t)) &&
           !overlap(device, sizeof(*device), config, sizeof(*config)) &&
           span(config->spi, sizeof(*config->spi),
                _Alignof(nx_spi_endpoint_t)) &&
           !overlap(device, sizeof(*device), config->spi,
                    sizeof(*config->spi)) &&
           config->spi->ops != NULL && config->spi->context != NULL &&
           config->spi->ops->transfer != NULL && config->clock.read != NULL &&
           config->clock_domain != 0U && config->clock_epoch != 0U;
}

nx_result_t nx_as5047p_init(nx_as5047p_t* device,
                            const nx_as5047p_config_t* config,
                            nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_as5047p_t)) ||
        !binding(device, config) || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered || device->initialized) {
        return NX_ERROR_BUSY;
    }
    if (device->config != NULL && device->config != config) {
        return NX_ERROR_STATE;
    }
    device->config = config;
    device->entered = true;
    uint16_t value;
    nx_result_t result =
        read_register(device, 0x0018U, false, &value, deadline);
    if (result == NX_SUCCESS &&
        ((value & 1U) == 0U ||
         (config->compensated && (value & 0x0050U) != 0U))) {
        result = NX_ERROR_STATE;
    }
    if (result == NX_SUCCESS) {
        result = diagnostics(device, &value, deadline);
    }
    device->initialized = result == NX_SUCCESS;
    device->uncertain = result != NX_SUCCESS && result != NX_ERROR_BUSY;
    device->entered = false;
    return result;
}

nx_result_t nx_as5047p_read(nx_as5047p_t* device, nx_as5047p_sample_t* sample,
                            nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_as5047p_t)) ||
        !span(sample, sizeof(*sample), _Alignof(nx_as5047p_sample_t)) ||
        overlap(device, sizeof(*device), sample, sizeof(*sample)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized || device->uncertain) {
        return NX_ERROR_STATE;
    }
    if (overlap(device->config, sizeof(*device->config), sample,
                sizeof(*sample)) ||
        overlap(device->config->spi, sizeof(*device->config->spi), sample,
                sizeof(*sample))) {
        return NX_ERROR_INVALID;
    }
    device->entered = true;
    nx_result_t result = observe(device, deadline);
    if (result == NX_SUCCESS) {
        nx_as5047p_sample_t output = {0};
        result = diagnostics(device, &output.diagnostics, deadline);
        if (result == NX_SUCCESS) {
            result = read_register(
                device, device->config->compensated ? 0x3fffU : 0x3ffeU, false,
                &output.raw_angle, deadline);
        }
        if (result == NX_SUCCESS) {
            output.observed = (nx_io_timestamp_t){
                .ticks = device->last_clock,
                .ticks_per_second = 1000000U,
                .domain = device->config->clock_domain,
                .epoch = device->config->clock_epoch,
                .source = NX_IO_TIMESTAMP_SERVICE_OBSERVATION};
            *sample = output;
        } else if (result != NX_ERROR_BUSY) {
            device->initialized = false;
            device->uncertain = true;
        }
    } else if (result == NX_ERROR_STATE) {
        device->initialized = false;
        device->uncertain = true;
    }
    device->entered = false;
    return result;
}

nx_result_t nx_as5047p_clear_error(nx_as5047p_t* device, uint16_t* flags,
                                   nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_as5047p_t)) ||
        !span(flags, sizeof(*flags), _Alignof(uint16_t)) ||
        overlap(device, sizeof(*device), flags, sizeof(*flags)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    if (device->config == NULL) {
        return NX_ERROR_STATE;
    }
    if (overlap(device->config, sizeof(*device->config), flags,
                sizeof(*flags)) ||
        overlap(device->config->spi, sizeof(*device->config->spi), flags,
                sizeof(*flags))) {
        return NX_ERROR_INVALID;
    }
    device->entered = true;
    uint16_t original;
    uint16_t remaining;
    nx_result_t result = read_register(device, 1U, true, &original, deadline);
    if (result == NX_SUCCESS) {
        result = read_register(device, 1U, false, &remaining, deadline);
    }
    if (result == NX_SUCCESS && remaining != 0U) {
        result = NX_ERROR_IO;
    }
    if (result == NX_SUCCESS) {
        *flags = original & 7U;
    }
    device->initialized = false;
    device->uncertain = result != NX_SUCCESS;
    device->entered = false;
    return result;
}

nx_result_t nx_as5047p_stop(nx_as5047p_t* device) {
    if (!span(device, sizeof(*device), _Alignof(nx_as5047p_t))) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    memset(device, 0, sizeof(*device));
    return NX_SUCCESS;
}
