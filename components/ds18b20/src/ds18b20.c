/**
 * \file            ds18b20.c
 * \brief           CRC-checked DS18B20 cooperative conversion owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ds18b20.h"

/** \brief Validate finite naturally aligned spans before typed access. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}
/** \brief All callers first validate both complete half-open spans. */
static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    return (uintptr_t)first < (uintptr_t)second + second_bytes &&
           (uintptr_t)second < (uintptr_t)first + first_bytes;
}
static bool owner_valid(const nx_ds18b20_t* device) {
    return span(device, sizeof(*device), _Alignof(nx_ds18b20_t));
}
/** \brief A configured ROM has a fixed family and its full CRC residue. */
static bool rom_valid(const uint8_t rom[8]) {
    return rom[0] == 0x28U && nx_onewire_crc8(rom, 8U) == 0U;
}
static bool config_valid(const nx_ds18b20_t* d, const nx_ds18b20_config_t* c) {
    return span(c, sizeof(*c), _Alignof(nx_ds18b20_config_t)) &&
           !overlap(d, sizeof(*d), c, sizeof(*c)) &&
           span(c->bus, sizeof(*c->bus), _Alignof(nx_onewire_t)) &&
           !overlap(d, sizeof(*d), c->bus, sizeof(*c->bus)) &&
           !overlap(c, sizeof(*c), c->bus, sizeof(*c->bus)) &&
           (!c->skip_rom || c->single_device);
}
/** \brief Protect the immutable wire facts whose complete types are known.
 * \note Opaque GPIO/clock context extents remain an assembly obligation.
 */
static bool outside_wire_facts(const nx_onewire_t* bus, const int16_t* out) {
    const nx_onewire_config_t* c = bus->config;
    if (c == NULL)
        return true;
    if (!span(c, sizeof(*c), _Alignof(nx_onewire_config_t)) ||
        !span(c->port, sizeof(*c->port), _Alignof(nx_gpio_port_t)) ||
        !span(c->port->ops, sizeof(*c->port->ops), _Alignof(nx_gpio_ops_t)))
        return false;
    return !overlap(c, sizeof(*c), out, sizeof(*out)) &&
           !overlap(c->port, sizeof(*c->port), out, sizeof(*out)) &&
           !overlap(c->port->ops, sizeof(*c->port->ops), out, sizeof(*out));
}
/** \brief Outputs cannot corrupt any known borrowed identity or owner state. */
static bool output_valid(const nx_ds18b20_t* d, const int16_t* out) {
    return span(out, sizeof(*out), _Alignof(int16_t)) &&
           !overlap(d, sizeof(*d), out, sizeof(*out)) &&
           (d->config == NULL ||
            (!overlap(d->config, sizeof(*d->config), out, sizeof(*out)) &&
             !overlap(d->config->bus, sizeof(*d->config->bus), out,
                      sizeof(*out)) &&
             outside_wire_facts(d->config->bus, out)));
}
/** \brief Close only CPU admission; autonomous conversion may keep running. */
static void close_device(nx_ds18b20_t* d) {
    d->initialized = false;
    d->converting = false;
    d->config = NULL;
    d->in_call = false;
}
/** \brief Guard each communication against its finite monotonic budget.
 * \note The wire owner enforces the borrowed clock epoch between calls.
 *       This local bound additionally rejects reversal within this call.
 */
static nx_result_t observe(nx_ds18b20_t* d, nx_time_us_t deadline,
                           nx_time_us_t* previous) {
    nx_time_us_t now;
    nx_result_t result = nx_onewire_now(d->config->bus, &now);
    if (result != NX_SUCCESS)
        return result;
    if (now == NX_DEADLINE_NEVER || now < *previous)
        return NX_ERROR_IO;
    *previous = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}
/** \brief Every function command gets a fresh reset and exact ROM selection. */
static nx_result_t selected(nx_ds18b20_t* d, uint8_t command, uint8_t* rx,
                            size_t bits, nx_time_us_t deadline) {
    uint8_t bytes[10];
    size_t count;
    if (d->config->skip_rom) {
        bytes[0] = 0xccU;
        bytes[1] = command;
        count = 2U;
    } else {
        bytes[0] = 0x55U;
        for (size_t i = 0U; i < 8U; ++i)
            bytes[i + 1U] = d->rom[i];
        bytes[9] = command;
        count = 10U;
    }
    return nx_onewire_exchange(d->config->bus, bytes, count, rx, bits,
                               deadline);
}
/** \brief Skip mode proves that its sole participant was not replaced. */
static nx_result_t read_identity(nx_ds18b20_t* d, nx_time_us_t deadline,
                                 bool initial) {
    if (!d->config->skip_rom)
        return NX_SUCCESS;
    const uint8_t command = 0x33U;
    uint8_t rom[8];
    nx_result_t result =
        nx_onewire_exchange(d->config->bus, &command, 1U, rom, 64U, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (!rom_valid(rom))
        return NX_ERROR_IO;
    for (size_t i = 0U; i < 8U; ++i) {
        if (!initial && rom[i] != d->rom[i])
            return NX_ERROR_STATE;
    }
    if (initial) {
        for (size_t i = 0U; i < 8U; ++i)
            d->rom[i] = rom[i];
    }
    return NX_SUCCESS;
}
/** \brief Decode only meaningful resolution bits; reserved byte6 is not trim.
 */
static nx_result_t scratchpad(nx_ds18b20_t* d, nx_time_us_t deadline,
                              uint8_t* resolution, int16_t* raw) {
    uint8_t bytes[9];
    nx_result_t result = selected(d, 0xbeU, bytes, 72U, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (nx_onewire_crc8(bytes, sizeof(bytes)) != 0U)
        return NX_ERROR_IO;
    if ((bytes[4] & 0x9fU) != 0x1fU)
        return NX_ERROR_STATE;
    uint8_t bits = (uint8_t)(9U + ((bytes[4] >> 5U) & 3U));
    uint32_t encoded = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8U;
    /* Lower-resolution LSBs are unspecified. Clear them before two's
     * complement decoding; signed right shift is not a portable rounding rule.
     */
    encoded &= ~((UINT32_C(1) << (12U - bits)) - 1U);
    int32_t value = (encoded & 0x8000U) != 0U
                        ? (int32_t)encoded - INT32_C(65536)
                        : (int32_t)encoded;
    if (value < -880 || value > 2000)
        return NX_ERROR_STATE;
    *resolution = bits;
    *raw = (int16_t)value;
    return NX_SUCCESS;
}
static uint32_t conversion_us(uint8_t resolution) {
    switch (resolution) {
        case 9U:
            return 93750U;
        case 10U:
            return 187500U;
        case 11U:
            return 375000U;
        default:
            return 750000U;
    }
}

nx_result_t nx_ds18b20_init(nx_ds18b20_t* d, const nx_ds18b20_config_t* c,
                            nx_time_us_t deadline) {
    if (!owner_valid(d))
        return NX_ERROR_INVALID;
    if (d->in_call)
        return NX_ERROR_BUSY;
    if (!config_valid(d, c) || deadline == NX_DEADLINE_NEVER ||
        (!c->skip_rom && !rom_valid(c->rom)))
        return NX_ERROR_INVALID;
    if (!c->external_vdd)
        return NX_ERROR_UNSUPPORTED;
    if (d->initialized || d->config != NULL)
        return NX_ERROR_BUSY;
    d->config = c;
    d->in_call = true;
    nx_time_us_t now = 0U;
    nx_result_t result = observe(d, deadline, &now);
    if (result == NX_SUCCESS && !c->skip_rom) {
        for (size_t i = 0U; i < 8U; ++i)
            d->rom[i] = c->rom[i];
    }
    if (result == NX_SUCCESS)
        result = read_identity(d, deadline, true);
    uint8_t supply = 0U;
    if (result == NX_SUCCESS)
        result = selected(d, 0xb4U, &supply, 1U, deadline);
    if (result == NX_SUCCESS && (supply & 1U) == 0U)
        result = NX_ERROR_UNSUPPORTED;
    uint8_t resolution = 0U;
    int16_t initial_temperature;
    if (result == NX_SUCCESS)
        result = scratchpad(d, deadline, &resolution, &initial_temperature);
    if (result == NX_SUCCESS)
        result = observe(d, deadline, &now);
    if (result != NX_SUCCESS) {
        close_device(d);
        return result;
    }
    d->resolution_bits = resolution;
    d->initialized = true;
    d->converting = false;
    d->in_call = false;
    return NX_SUCCESS;
}
nx_result_t nx_ds18b20_start(nx_ds18b20_t* d, nx_time_us_t deadline) {
    if (!owner_valid(d) || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (d->in_call || d->converting)
        return NX_ERROR_BUSY;
    if (!d->initialized || d->config == NULL)
        return NX_ERROR_STATE;
    d->in_call = true;
    nx_time_us_t now = 0U;
    nx_result_t result = observe(d, deadline, &now);
    if (result == NX_SUCCESS)
        result = read_identity(d, deadline, false);
    uint8_t resolution = 0U;
    int16_t previous_temperature;
    if (result == NX_SUCCESS)
        result = scratchpad(d, deadline, &resolution, &previous_temperature);
    if (result == NX_SUCCESS)
        result = observe(d, deadline, &now);
    if (result == NX_SUCCESS)
        result = selected(d, 0x44U, NULL, 0U, deadline);
    if (result == NX_SUCCESS)
        result = observe(d, deadline, &now);
    uint32_t duration = conversion_us(resolution);
    if (result == NX_SUCCESS && duration >= NX_DEADLINE_NEVER - now)
        result = NX_ERROR_EXHAUSTED;
    if (result != NX_SUCCESS) {
        close_device(d);
        return result;
    }
    d->ready_at = now + duration;
    d->deadline = deadline;
    d->resolution_bits = resolution;
    d->converting = true;
    d->in_call = false;
    return NX_SUCCESS;
}
nx_result_t nx_ds18b20_service(nx_ds18b20_t* d, int16_t* out) {
    if (!owner_valid(d))
        return NX_ERROR_INVALID;
    if (d->in_call)
        return NX_ERROR_BUSY;
    if (!output_valid(d, out))
        return NX_ERROR_INVALID;
    if (!d->initialized || !d->converting || d->config == NULL)
        return NX_ERROR_STATE;
    d->in_call = true;
    nx_time_us_t now = 0U;
    nx_result_t result = observe(d, d->deadline, &now);
    if (result == NX_SUCCESS && now < d->ready_at) {
        d->in_call = false;
        return NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS)
        result = read_identity(d, d->deadline, false);
    uint8_t resolution = 0U;
    int16_t raw = 0;
    if (result == NX_SUCCESS)
        result = scratchpad(d, d->deadline, &resolution, &raw);
    if (result == NX_SUCCESS && resolution != d->resolution_bits)
        result = NX_ERROR_STATE;
    if (result == NX_SUCCESS)
        result = observe(d, d->deadline, &now);
    if (result != NX_SUCCESS) {
        close_device(d);
        return result;
    }
    d->converting = false;
    d->in_call = false;
    *out = raw;
    return NX_SUCCESS;
}
nx_result_t nx_ds18b20_stop(nx_ds18b20_t* d) {
    if (!owner_valid(d))
        return NX_ERROR_INVALID;
    if (d->in_call)
        return NX_ERROR_BUSY;
    close_device(d);
    return NX_SUCCESS;
}
