/**
 * \file            bmp280.c
 * \brief           BMP280 datasheet register and integer compensation core
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bmp280.h"
#include <limits.h>
#include <string.h>

/** \brief Decode little-endian without alignment or host-endian assumptions. */
static uint16_t little_u16(const uint8_t* bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

/** \brief Decode signed coefficients without implementation-defined casts. */
static int16_t little_i16(const uint8_t* bytes) {
    uint16_t value = little_u16(bytes);
    int32_t decoded =
        value <= INT16_MAX ? (int32_t)value : (int32_t)value - 65536;
    return (int16_t)decoded;
}

/** \brief Parse the actual BMP280 coefficient layout. */
nx_result_t nx_bmp280_parse_calibration(const uint8_t* bytes, size_t length,
                                        nx_bmp280_calibration_t* calibration) {
    if (bytes == NULL || calibration == NULL || length != 24) {
        return NX_ERROR_INVALID;
    }
    nx_bmp280_calibration_t parsed = {
        little_u16(bytes),      little_i16(bytes + 2),  little_i16(bytes + 4),
        little_u16(bytes + 6),  little_i16(bytes + 8),  little_i16(bytes + 10),
        little_i16(bytes + 12), little_i16(bytes + 14), little_i16(bytes + 16),
        little_i16(bytes + 18), little_i16(bytes + 20), little_i16(bytes + 22)};
    if (parsed.t1 == 0 || parsed.p1 == 0) {
        return NX_ERROR_INVALID;
    }
    *calibration = parsed;
    return NX_SUCCESS;
}

/** \brief Reject corrupted calibration overflow instead of invoking C UB. */
nx_result_t nx_bmp280_compensate(const nx_bmp280_calibration_t* calibration,
                                 uint32_t raw_temperature,
                                 uint32_t raw_pressure,
                                 nx_bmp280_sample_t* sample) {
    if (calibration == NULL || sample == NULL || calibration->p1 == 0 ||
        raw_temperature > 0xfffffu || raw_pressure > 0xfffffu) {
        return NX_ERROR_INVALID;
    }
    if (raw_temperature == 0x80000u || raw_pressure == 0x80000u) {
        return NX_ERROR_IO;
    }
    const nx_bmp280_calibration_t* c = calibration;
    int64_t delta = (int64_t)(raw_temperature >> 4) - c->t1;
    int64_t first =
        (((int64_t)(raw_temperature >> 3) - (int64_t)c->t1 * 2) * c->t2) >> 11;
    int64_t second = (((delta * delta) >> 12) * c->t3) >> 14;
    int64_t fine = first + second;
    int64_t temperature = (fine * 5 + 128) >> 8;
    if (temperature < -4000 || temperature > 8500) {
        return NX_ERROR_IO;
    }
    int64_t var1 = fine - 128000;
    int64_t square = var1 * var1;
    int64_t var2 =
        square * c->p6 + var1 * c->p5 * 131072 + (int64_t)c->p4 * 34359738368LL;
    var1 = ((square * c->p3) >> 8) + var1 * c->p2 * 4096;
    if (__builtin_mul_overflow(140737488355328LL + var1, (int64_t)c->p1,
                               &var1)) {
        return NX_ERROR_INVALID;
    }
    var1 >>= 33;
    if (var1 <= 0) {
        return NX_ERROR_INVALID;
    }
    int64_t pressure = (1048576LL - raw_pressure) * 2147483648LL - var2;
    if (__builtin_mul_overflow(pressure, 3125LL, &pressure)) {
        return NX_ERROR_INVALID;
    }
    pressure /= var1;
    int64_t scaled = pressure >> 13;
    int64_t product;
    if (__builtin_mul_overflow(scaled, scaled, &product) ||
        __builtin_mul_overflow(product, (int64_t)c->p9, &product)) {
        return NX_ERROR_INVALID;
    }
    var1 = product >> 25;
    if (__builtin_mul_overflow((int64_t)c->p8, pressure, &product)) {
        return NX_ERROR_INVALID;
    }
    var2 = product >> 19;
    if (__builtin_add_overflow(pressure, var1, &pressure) ||
        __builtin_add_overflow(pressure, var2, &pressure)) {
        return NX_ERROR_INVALID;
    }
    pressure = (pressure >> 8) + (int64_t)c->p7 * 16;
    pressure /= 256;
    if (pressure < 30000 || pressure > 110000) {
        return NX_ERROR_IO;
    }
    nx_bmp280_sample_t result = {(int32_t)temperature, (uint32_t)pressure};
    *sample = result;
    return NX_SUCCESS;
}

/** \brief Read at most the calibration block in one settled CS transaction. */
static nx_result_t read_registers(nx_bmp280_t* device, uint8_t address,
                                  uint8_t* bytes, size_t length,
                                  nx_time_us_t deadline) {
    uint8_t tx[25];
    uint8_t rx[25];
    if (length == 0 || length > 24) {
        return NX_ERROR_INVALID;
    }
    memset(tx, 0xff, length + 1);
    tx[0] = address | 0x80u;
    nx_result_t result = device->transport.exchange(
        device->transport.context, tx, rx, length + 1, deadline);
    if (result == NX_SUCCESS &&
        nx_deadline_expired(deadline, device->transport.clock.read(
                                          device->transport.clock.context))) {
        result = NX_ERROR_TIMEOUT;
    }
    if (result == NX_SUCCESS) {
        memcpy(bytes, rx + 1, length);
    }
    return result;
}

/** \brief Execute one register write with no retained stack buffer. */
static nx_result_t write_register(nx_bmp280_t* device, uint8_t address,
                                  uint8_t value, nx_time_us_t deadline) {
    uint8_t tx[2] = {address & 0x7fu, value};
    nx_result_t result = device->transport.exchange(
        device->transport.context, tx, NULL, sizeof(tx), deadline);
    return result == NX_SUCCESS &&
                   nx_deadline_expired(deadline,
                                       device->transport.clock.read(
                                           device->transport.clock.context))
               ? NX_ERROR_TIMEOUT
               : result;
}

/** \brief Read exact ID/status/calibration before exposing a live instance. */
nx_result_t nx_bmp280_init(nx_bmp280_t* device,
                           nx_bmp280_transport_port_t transport,
                           nx_time_us_t deadline) {
    if (device == NULL || transport.exchange == NULL ||
        transport.clock.read == NULL || device->measuring ||
        device->initialized) {
        return NX_ERROR_INVALID;
    }
    device->transport = transport;
    if (nx_deadline_expired(deadline,
                            transport.clock.read(transport.clock.context))) {
        return NX_ERROR_TIMEOUT;
    }
    uint8_t value;
    nx_result_t result = read_registers(device, 0xd0, &value, 1, deadline);
    if (result != NX_SUCCESS || value != 0x58) {
        return result != NX_SUCCESS ? result : NX_ERROR_UNSUPPORTED;
    }
    result = read_registers(device, 0xf3, &value, 1, deadline);
    if (result != NX_SUCCESS || (value & 9u) != 0) {
        return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
    }
    uint8_t calibration[24];
    result = read_registers(device, 0x88, calibration, sizeof(calibration),
                            deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_bmp280_parse_calibration(calibration, sizeof(calibration),
                                         &device->calibration);
    if (result == NX_SUCCESS) {
        result = write_register(device, 0xf5, 0, deadline);
    }
    if (result == NX_SUCCESS) {
        device->initialized = true;
    }
    return result;
}

/** \brief Start explicitly selected x1/x1 forced conversion. */
nx_result_t nx_bmp280_begin(nx_bmp280_t* device, nx_time_us_t deadline) {
    if (device == NULL || !device->initialized) {
        return NX_ERROR_STATE;
    }
    if (device->measuring) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, device->transport.clock.read(
                                          device->transport.clock.context))) {
        return NX_ERROR_TIMEOUT;
    }
    uint8_t status;
    nx_result_t result = read_registers(device, 0xf3, &status, 1, deadline);
    if (result != NX_SUCCESS || (status & 9u) != 0) {
        return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
    }
    result = write_register(device, 0xf4, 0x25, deadline);
    if (result == NX_SUCCESS) {
        device->deadline = deadline;
        device->earliest_ready = nx_deadline_after(
            device->transport.clock.read(device->transport.clock.context),
            8000);
        device->measuring = true;
    }
    return result;
}

/** \brief Observe status, then compensate without hiding a sampling task. */
nx_result_t nx_bmp280_poll(nx_bmp280_t* device, nx_bmp280_sample_t* sample) {
    if (device == NULL || sample == NULL || !device->initialized ||
        !device->measuring) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(
            device->deadline,
            device->transport.clock.read(device->transport.clock.context))) {
        device->measuring = false;
        return NX_ERROR_TIMEOUT;
    }
    if (device->transport.clock.read(device->transport.clock.context) <
        device->earliest_ready) {
        return NX_ERROR_BUSY;
    }
    uint8_t status;
    nx_result_t result =
        read_registers(device, 0xf3, &status, 1, device->deadline);
    if (result == NX_SUCCESS && (status & 8u) != 0) {
        return NX_ERROR_BUSY;
    }
    uint8_t raw[6];
    if (result == NX_SUCCESS) {
        result =
            read_registers(device, 0xf7, raw, sizeof(raw), device->deadline);
        if (result == NX_SUCCESS) {
            uint32_t pressure = ((uint32_t)raw[0] << 12) |
                                ((uint32_t)raw[1] << 4) | (raw[2] >> 4);
            uint32_t temperature = ((uint32_t)raw[3] << 12) |
                                   ((uint32_t)raw[4] << 4) | (raw[5] >> 4);
            result = nx_bmp280_compensate(&device->calibration, temperature,
                                          pressure, sample);
        }
    }
    device->measuring = false;
    return result;
}

/** \brief Release only software state; physical power/reset stays external. */
void nx_bmp280_stop(nx_bmp280_t* device) {
    if (device != NULL) {
        device->initialized = false;
        device->measuring = false;
    }
}
