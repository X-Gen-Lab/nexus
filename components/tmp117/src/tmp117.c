/**
 * \file            tmp117.c
 * \brief           TMP117 one-shot protocol using fully settled exchanges
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/tmp117.h"
#include <string.h>

#define TMP117_CONFIGURATION 0x01U
#define TMP117_EEPROM        0x04U
#define TMP117_OFFSET        0x07U
#define TMP117_ID            0x0fU
#define TMP117_READY         0x2000U
#define TMP117_EEPROM_BUSY   0x1000U
#define TMP117_EUN           0x8000U
#define TMP117_EBUSY         0x4000U
#define TMP117_MODE          0x0c00U
#define TMP117_SHUTDOWN      0x0400U
#define TMP117_ONESHOT       0x0c00U
#define TMP117_CONTROL       0x0ffcU
#define TMP117_PRESERVE      0x039cU

/** \brief Validate numeric spans before reading typed control storage. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Known spans cannot alias; opaque context extents stay external. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Retained uncertainty can only be resynchronized on the same ports. */
static bool same_binding(const nx_tmp117_t* device,
                         nx_tmp117_transport_port_t port,
                         const nx_tmp117_config_t* config) {
    return device->transport.exchange == NULL ||
           (device->transport.context == port.context &&
            device->transport.exchange == port.exchange &&
            device->transport.clock.read == port.clock.read &&
            device->transport.clock.context == port.clock.context &&
            device->config.clock_domain == config->clock_domain &&
            device->config.clock_epoch == config->clock_epoch &&
            device->config.expected_offset_lsb == config->expected_offset_lsb);
}

/** \brief Every callback is enclosed by the public serialized-call guard. */
static nx_result_t observe_clock(nx_tmp117_t* device, nx_time_us_t deadline) {
    nx_time_us_t now =
        device->transport.clock.read(device->transport.clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        return NX_ERROR_IO;
    }
    device->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief No register access can retain the local TX/RX arrays. */
static nx_result_t read_word(nx_tmp117_t* device, uint8_t reg, uint16_t* word,
                             nx_time_us_t deadline) {
    nx_result_t result = observe_clock(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint8_t bytes[2];
    result = device->transport.exchange(device->transport.context, &reg, 1U,
                                        bytes, sizeof(bytes), deadline);
    nx_result_t timing = observe_clock(device, deadline);
    if (result != NX_SUCCESS || timing != NX_SUCCESS) {
        return result != NX_SUCCESS ? result : timing;
    }
    *word = (uint16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
    return NX_SUCCESS;
}

/** \brief A failed write may have reached the device despite bus failure. */
static nx_result_t write_configuration(nx_tmp117_t* device, uint16_t value,
                                       nx_time_us_t deadline, bool* issued) {
    nx_result_t result = observe_clock(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint8_t bytes[3] = {TMP117_CONFIGURATION, (uint8_t)(value >> 8U),
                              (uint8_t)value};
    if (issued != NULL) {
        *issued = true;
    }
    result = device->transport.exchange(device->transport.context, bytes,
                                        sizeof(bytes), NULL, 0U, deadline);
    nx_result_t timing = observe_clock(device, deadline);
    return result != NX_SUCCESS ? result : timing;
}

/** \brief Decode two's complement without implementation-defined narrowing. */
static int32_t signed_word(uint16_t word) {
    return word <= 0x7fffU ? (int32_t)word : (int32_t)word - 65536;
}

/** \brief Unknown physical state closes publication without revoking a loan. */
static nx_result_t fail(nx_tmp117_t* device, nx_result_t result) {
    device->initialized = false;
    device->measuring = false;
    device->uncertain = true;
    return result;
}

/** \brief EEPROM must remain locked before any volatile configuration write. */
static nx_result_t eeprom_ready(nx_tmp117_t* device, nx_time_us_t deadline) {
    uint16_t value;
    nx_result_t result = read_word(device, TMP117_EEPROM, &value, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((value & TMP117_EBUSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    return (value & TMP117_EUN) != 0U ? NX_ERROR_PERMISSION : NX_SUCCESS;
}

nx_result_t nx_tmp117_init(nx_tmp117_t* device,
                           nx_tmp117_transport_port_t transport,
                           const nx_tmp117_config_t* config,
                           nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_tmp117_t)) ||
        !span(config, sizeof(*config), _Alignof(nx_tmp117_config_t)) ||
        overlaps(device, sizeof(*device), config, sizeof(*config)) ||
        transport.context == NULL || transport.exchange == NULL ||
        transport.clock.read == NULL || config->clock_domain == 0U ||
        config->clock_epoch == 0U || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call || device->initialized || device->measuring) {
        return NX_ERROR_BUSY;
    }
    if (!same_binding(device, transport, config)) {
        return NX_ERROR_STATE;
    }
    device->in_call = true;
    nx_tmp117_t candidate = {.transport = transport,
                             .config = *config,
                             .last_clock = device->last_clock};
    uint16_t value;
    nx_result_t result = read_word(&candidate, TMP117_ID, &value, deadline);
    if (result == NX_SUCCESS && value != 0x0117U) {
        result = NX_ERROR_UNSUPPORTED;
    }
    uint16_t configuration = 0U;
    if (result == NX_SUCCESS) {
        result = read_word(&candidate, TMP117_CONFIGURATION, &configuration,
                           deadline);
        if (result == NX_SUCCESS &&
            (configuration & TMP117_EEPROM_BUSY) != 0U) {
            result = NX_ERROR_BUSY;
        }
    }
    if (result == NX_SUCCESS) {
        result = eeprom_ready(&candidate, deadline);
    }
    if (result == NX_SUCCESS) {
        result = read_word(&candidate, TMP117_OFFSET, &value, deadline);
        if (result == NX_SUCCESS &&
            signed_word(value) != config->expected_offset_lsb) {
            result = NX_ERROR_STATE;
        }
    }
    candidate.base_configuration = configuration & TMP117_PRESERVE;
    uint16_t shutdown = candidate.base_configuration | TMP117_SHUTDOWN;
    bool shutdown_issued = false;
    if (result == NX_SUCCESS) {
        result = write_configuration(&candidate, shutdown, deadline,
                                     &shutdown_issued);
    }
    if (result == NX_SUCCESS) {
        result = read_word(&candidate, TMP117_CONFIGURATION, &value, deadline);
        if (result == NX_SUCCESS && ((value & TMP117_CONTROL) != shutdown ||
                                     (value & TMP117_EEPROM_BUSY) != 0U)) {
            result = NX_ERROR_STATE;
        }
    }
    if (result == NX_SUCCESS) {
        candidate.initialized = true;
        *device = candidate;
    } else if (shutdown_issued) {
        /* An issued write keeps its recovery binding despite failed init. */
        candidate.uncertain = true;
        *device = candidate;
    } else {
        device->in_call = false;
    }
    return result;
}

nx_result_t nx_tmp117_begin(nx_tmp117_t* device, nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_tmp117_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized || device->uncertain) {
        return NX_ERROR_STATE;
    }
    if (device->measuring) {
        return NX_ERROR_BUSY;
    }
    device->in_call = true;
    nx_result_t result = observe_clock(device, deadline);
    if (result == NX_SUCCESS) {
        uint16_t value;
        result = read_word(device, TMP117_CONFIGURATION, &value, deadline);
        if (result == NX_SUCCESS &&
            ((value & TMP117_CONTROL) !=
                 (device->base_configuration | TMP117_SHUTDOWN) ||
             (value & TMP117_EEPROM_BUSY) != 0U)) {
            result = NX_ERROR_STATE;
        }
        if (result == NX_SUCCESS) {
            device->conversion_started_at = device->last_clock;
            result = write_configuration(
                device, device->base_configuration | TMP117_ONESHOT, deadline,
                NULL);
        }
        if (result == NX_SUCCESS) {
            device->deadline = deadline;
            device->measuring = true;
        } else {
            result = fail(device, result);
        }
    } else if (result == NX_ERROR_IO) {
        result = fail(device, result);
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_tmp117_poll(nx_tmp117_t* device, nx_tmp117_sample_t* sample) {
    if (!span(device, sizeof(*device), _Alignof(nx_tmp117_t)) ||
        !span(sample, sizeof(*sample), _Alignof(nx_tmp117_sample_t)) ||
        overlaps(device, sizeof(*device), sample, sizeof(*sample))) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized || !device->measuring || device->uncertain) {
        return NX_ERROR_STATE;
    }
    device->in_call = true;
    uint16_t value;
    nx_result_t result =
        read_word(device, TMP117_CONFIGURATION, &value, device->deadline);
    if (result == NX_SUCCESS) {
        uint16_t mode = value & TMP117_MODE;
        if ((value & (TMP117_CONTROL & ~TMP117_MODE)) !=
                device->base_configuration ||
            (value & TMP117_EEPROM_BUSY) != 0U ||
            (mode != TMP117_ONESHOT && mode != TMP117_SHUTDOWN)) {
            result = NX_ERROR_STATE;
        } else if ((value & TMP117_READY) == 0U) {
            device->in_call = false;
            return NX_ERROR_BUSY;
        }
    }
    if (result == NX_SUCCESS) {
        result = read_word(device, 0U, &value, device->deadline);
    }
    if (result == NX_SUCCESS) {
        int32_t raw = signed_word(value);
        if (raw < -7040 || raw > 19200) {
            result = NX_ERROR_IO;
        } else {
            /* |raw * 15625| <= 512000000 for the entire 16-bit format. */
            int32_t scaled = raw * 15625;
            nx_tmp117_sample_t output = {0};
            output.micro_celsius = (scaled + (scaled < 0 ? -1 : 1)) / 2;
            output.raw_lsb = (int16_t)raw;
            output.offset_lsb = device->config.expected_offset_lsb;
            output.conversion_started_at = device->conversion_started_at;
            output.observed = (nx_io_timestamp_t){
                .ticks = device->last_clock,
                .ticks_per_second = 1000000U,
                .domain = device->config.clock_domain,
                .epoch = device->config.clock_epoch,
                .source = NX_IO_TIMESTAMP_SERVICE_OBSERVATION};
            device->measuring = false;
            *sample = output;
        }
    }
    if (result != NX_SUCCESS) {
        result = fail(device, result);
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_tmp117_cancel(nx_tmp117_t* device, nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_tmp117_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call) {
        return NX_ERROR_BUSY;
    }
    if (device->transport.exchange == NULL ||
        (!device->initialized && !device->uncertain && !device->measuring)) {
        return NX_ERROR_STATE;
    }
    device->in_call = true;
    nx_result_t result = observe_clock(device, deadline);
    if (result == NX_SUCCESS) {
        result = eeprom_ready(device, deadline);
        if (result == NX_ERROR_BUSY) {
            device->in_call = false;
            return result;
        }
        uint16_t shutdown = device->base_configuration | TMP117_SHUTDOWN;
        if (result == NX_SUCCESS) {
            result = write_configuration(device, shutdown, deadline, NULL);
        }
        uint16_t value;
        if (result == NX_SUCCESS) {
            result = read_word(device, TMP117_CONFIGURATION, &value, deadline);
            if (result == NX_SUCCESS && ((value & TMP117_CONTROL) != shutdown ||
                                         (value & TMP117_EEPROM_BUSY) != 0U)) {
                result = NX_ERROR_STATE;
            }
        }
        if (result == NX_SUCCESS) {
            device->measuring = false;
            device->uncertain = false;
        } else {
            result = fail(device, result);
        }
    } else if (result == NX_ERROR_IO) {
        result = fail(device, result);
    }
    device->in_call = false;
    return result;
}

void nx_tmp117_stop(nx_tmp117_t* device) {
    if (span(device, sizeof(*device), _Alignof(nx_tmp117_t)) &&
        !device->in_call) {
        /* This does not resolve physical uncertainty owned by the caller. */
        memset(device, 0, sizeof(*device));
    }
}
