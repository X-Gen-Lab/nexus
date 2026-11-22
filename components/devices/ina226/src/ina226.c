/**
 * \file            ina226.c
 * \brief           Calibrated INA226 triggered measurements over public I2C
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ina226.h"
#include <string.h>

/** \brief Check typed storage before control or output access. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Known metadata and output spans must not overwrite one another. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return an > UINTPTR_MAX - first || bn > UINTPTR_MAX - second ||
           (first < second + bn && second < first + an);
}

nx_result_t nx_ina226_calibration(uint32_t shunt_microohms,
                                  uint32_t current_lsb_nanoamps,
                                  uint16_t* calibration) {
    if (shunt_microohms == 0U || current_lsb_nanoamps == 0U ||
        !span(calibration, sizeof(*calibration), _Alignof(uint16_t))) {
        return NX_ERROR_INVALID;
    }
    uint64_t divisor = (uint64_t)shunt_microohms * current_lsb_nanoamps;
    uint64_t result = UINT64_C(5120000000000) / divisor;
    if (result == 0U || result > 32767U) {
        return NX_ERROR_EXHAUSTED;
    }
    *calibration = (uint16_t)result;
    return NX_SUCCESS;
}

/** \brief Preserve one finite epoch and budget across every I2C exchange. */
static nx_result_t observe(nx_ina226_t* device, nx_time_us_t deadline) {
    nx_time_us_t now =
        device->config->clock.read(device->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        return NX_ERROR_STATE;
    }
    device->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief End every local descriptor/buffer borrow before returning. */
static nx_result_t exchange(nx_ina226_t* device, const uint8_t* tx,
                            size_t tx_bytes, uint8_t* rx,
                            nx_time_us_t deadline) {
    nx_result_t result = observe(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_i2c_message_t messages[2] = {nx_i2c_message_write(tx, tx_bytes),
                                    nx_i2c_message_read(rx, 2U)};
    size_t transferred = 0U;
    result = nx_i2c_endpoint_transaction(device->config->i2c, messages,
                                         rx == NULL ? 1U : 2U, deadline,
                                         &transferred);
    nx_result_t timing = observe(device, deadline);
    if (result != NX_SUCCESS || timing != NX_SUCCESS) {
        return result != NX_SUCCESS ? result : timing;
    }
    return transferred == tx_bytes + (rx == NULL ? 0U : 2U) ? NX_SUCCESS
                                                            : NX_ERROR_IO;
}

/** \brief Pointer write and repeated-START read preserve MSB-first words. */
static nx_result_t read_register(nx_ina226_t* device, uint8_t reg,
                                 uint16_t* value, nx_time_us_t deadline) {
    uint8_t bytes[2];
    nx_result_t result = exchange(device, &reg, 1U, bytes, deadline);
    if (result == NX_SUCCESS) {
        *value = (uint16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
    }
    return result;
}

/** \brief An issued configuration write can take effect before an error. */
static nx_result_t write_register(nx_ina226_t* device, uint8_t reg,
                                  uint16_t value, nx_time_us_t deadline) {
    const uint8_t bytes[3] = {reg, (uint8_t)(value >> 8U), (uint8_t)value};
    return exchange(device, bytes, sizeof(bytes), NULL, deadline);
}

/** \brief Never publish a sample from a changed/reset configuration. */
static nx_result_t verify(nx_ina226_t* device, nx_time_us_t deadline,
                          bool allow_triggered) {
    uint16_t value;
    nx_result_t result = read_register(device, 0U, &value, deadline);
    if (result == NX_SUCCESS && value != device->configuration &&
        !(allow_triggered && value == (uint16_t)(device->configuration | 3U))) {
        result = NX_ERROR_STATE;
    }
    if (result == NX_SUCCESS) {
        result = read_register(device, 5U, &value, deadline);
        if (result == NX_SUCCESS && value != device->calibration) {
            result = NX_ERROR_STATE;
        }
    }
    return result;
}

/** \brief Retain the exact owner after protocol or physical uncertainty. */
static nx_result_t failed(nx_ina226_t* device, nx_result_t result) {
    device->initialized = false;
    device->measuring = false;
    device->uncertain = true;
    return result;
}

nx_result_t nx_ina226_init(nx_ina226_t* device,
                           const nx_ina226_config_t* config,
                           nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_ina226_t)) ||
        !span(config, sizeof(*config), _Alignof(nx_ina226_config_t)) ||
        overlap(device, sizeof(*device), config, sizeof(*config)) ||
        !span(config->i2c, sizeof(*config->i2c), _Alignof(nx_i2c_endpoint_t)) ||
        overlap(device, sizeof(*device), config->i2c, sizeof(*config->i2c)) ||
        config->i2c->ops == NULL || config->i2c->context == NULL ||
        config->i2c->ops->transaction == NULL || config->clock.read == NULL ||
        config->clock_domain == 0U || config->clock_epoch == 0U ||
        config->averaging > 7U || config->bus_conversion > 7U ||
        config->shunt_conversion > 7U || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered || device->initialized || device->measuring) {
        return NX_ERROR_BUSY;
    }
    if (device->config != NULL && device->config != config) {
        return NX_ERROR_STATE;
    }
    nx_ina226_t candidate = {.config = config,
                             .last_clock = device->last_clock};
    nx_result_t result = nx_ina226_calibration(config->shunt_microohms,
                                               config->current_lsb_nanoamps,
                                               &candidate.calibration);
    if (result != NX_SUCCESS) {
        return result;
    }
    candidate.configuration =
        (uint16_t)(0x4000U | ((uint16_t)config->averaging << 9U) |
                   ((uint16_t)config->bus_conversion << 6U) |
                   ((uint16_t)config->shunt_conversion << 3U));
    device->entered = true;
    uint16_t value;
    result = read_register(&candidate, 0xfeU, &value, deadline);
    if (result == NX_SUCCESS && value != 0x5449U) {
        result = NX_ERROR_UNSUPPORTED;
    }
    if (result == NX_SUCCESS) {
        result = read_register(&candidate, 0xffU, &value, deadline);
        if (result == NX_SUCCESS && value != 0x2260U && value != 0x2261U) {
            result = NX_ERROR_UNSUPPORTED;
        }
    }
    if (result == NX_SUCCESS) {
        result = read_register(&candidate, 6U, &value, deadline);
        if (result == NX_SUCCESS && (value & 0xfc03U) != 0U) {
            result = NX_ERROR_STATE;
        }
    }
    bool issued = result == NX_SUCCESS;
    if (issued) {
        result =
            write_register(&candidate, 0U, candidate.configuration, deadline);
    }
    if (result == NX_SUCCESS) {
        result =
            write_register(&candidate, 5U, candidate.calibration, deadline);
    }
    if (result == NX_SUCCESS) {
        result = verify(&candidate, deadline, false);
    }
    if (result == NX_SUCCESS || issued) {
        candidate.initialized = result == NX_SUCCESS;
        candidate.uncertain = result != NX_SUCCESS;
        *device = candidate;
    } else {
        device->entered = false;
    }
    return result;
}

nx_result_t nx_ina226_begin(nx_ina226_t* device, nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_ina226_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered || device->measuring) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized || device->uncertain) {
        return NX_ERROR_STATE;
    }
    device->entered = true;
    nx_result_t result = observe(device, deadline);
    if (result == NX_SUCCESS) {
        result = verify(device, deadline, true);
        if (result == NX_SUCCESS) {
            device->started_at = device->last_clock;
            result = write_register(
                device, 0U, (uint16_t)(device->configuration | 3U), deadline);
        }
        if (result == NX_SUCCESS) {
            device->deadline = deadline;
            device->measuring = true;
        } else {
            result = failed(device, result);
        }
    } else if (result == NX_ERROR_STATE) {
        result = failed(device, result);
    }
    device->entered = false;
    return result;
}

/** \brief Convert two's complement without implementation-defined narrowing. */
static int32_t signed_word(uint16_t word) {
    return word <= 0x7fffU ? (int32_t)word : (int32_t)word - 65536;
}

nx_result_t nx_ina226_poll(nx_ina226_t* device, nx_ina226_sample_t* sample) {
    if (!span(device, sizeof(*device), _Alignof(nx_ina226_t)) ||
        !span(sample, sizeof(*sample), _Alignof(nx_ina226_sample_t)) ||
        overlap(device, sizeof(*device), sample, sizeof(*sample))) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized || !device->measuring || device->uncertain) {
        return NX_ERROR_STATE;
    }
    if (overlap(device->config, sizeof(*device->config), sample,
                sizeof(*sample)) ||
        overlap(device->config->i2c, sizeof(*device->config->i2c), sample,
                sizeof(*sample))) {
        return NX_ERROR_INVALID;
    }
    device->entered = true;
    nx_result_t result = verify(device, device->deadline, true);
    uint16_t status = 0U;
    if (result == NX_SUCCESS) {
        result = read_register(device, 6U, &status, device->deadline);
    }
    if (result == NX_SUCCESS && (status & 0x0004U) != 0U) {
        result = NX_ERROR_IO;
    }
    if (result == NX_SUCCESS && (status & 0x0008U) == 0U) {
        device->entered = false;
        return NX_ERROR_BUSY;
    }
    const uint8_t registers[4] = {1U, 2U, 4U, 3U};
    uint16_t words[4];
    for (size_t index = 0U; index < 4U && result == NX_SUCCESS; ++index) {
        result = read_register(device, registers[index], &words[index],
                               device->deadline);
    }
    if (result == NX_SUCCESS && words[1] > 28800U) {
        result = NX_ERROR_IO;
    }
    if (result == NX_SUCCESS) {
        nx_ina226_sample_t output = {
            .shunt_nanovolts = signed_word(words[0]) * 2500,
            .bus_microvolts = (uint32_t)words[1] * 1250U,
            .current_nanoamps = (int64_t)signed_word(words[2]) *
                                device->config->current_lsb_nanoamps,
            .power_nanowatts =
                (uint64_t)words[3] * 25U * device->config->current_lsb_nanoamps,
            .started_at = device->started_at,
            .observed = {.ticks = device->last_clock,
                         .ticks_per_second = 1000000U,
                         .domain = device->config->clock_domain,
                         .epoch = device->config->clock_epoch,
                         .source = NX_IO_TIMESTAMP_SERVICE_OBSERVATION}};
        device->measuring = false;
        *sample = output;
    } else {
        result = failed(device, result);
    }
    device->entered = false;
    return result;
}

nx_result_t nx_ina226_shutdown(nx_ina226_t* device, nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_ina226_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    if (device->config == NULL) {
        return NX_ERROR_STATE;
    }
    device->entered = true;
    nx_result_t result =
        write_register(device, 0U, device->configuration, deadline);
    uint16_t value;
    if (result == NX_SUCCESS) {
        result = read_register(device, 0U, &value, deadline);
        if (result == NX_SUCCESS && value != device->configuration) {
            result = NX_ERROR_STATE;
        }
    }
    device->initialized = false;
    device->measuring = false;
    device->uncertain = result != NX_SUCCESS;
    device->entered = false;
    return result;
}

nx_result_t nx_ina226_stop(nx_ina226_t* device) {
    if (!span(device, sizeof(*device), _Alignof(nx_ina226_t))) {
        return NX_ERROR_INVALID;
    }
    if (device->entered) {
        return NX_ERROR_BUSY;
    }
    if (device->measuring || device->uncertain) {
        return NX_ERROR_STATE;
    }
    memset(device, 0, sizeof(*device));
    return NX_SUCCESS;
}
