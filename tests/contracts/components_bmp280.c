/**
 * \file            components_bmp280.c
 * \brief           BMP280 real register/compensation contract fault tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bmp280.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            abort();                                                           \
        }                                                                      \
    } while (0)

/** \brief Datasheet example calibration with signed little-endian fields. */
static const nx_bmp280_calibration_t s_calibration = {
    27504, 26435, -1000, 36477, -10685, 3024,
    2855,  140,   -7,    15500, -14600, 6000};

/** \brief Synchronous transaction model verifies temporary-buffer lifetime. */
typedef struct {
    uint8_t registers[256];
    uint64_t now;
    unsigned calls;
    unsigned writes;
    bool borrowed;
    nx_result_t failure;
} fixture_t;

/** \brief Encode an exact signed/unsigned calibration word. */
static void put16(uint8_t* destination, int32_t value) {
    uint16_t bits = (uint16_t)value;
    destination[0] = (uint8_t)bits;
    destination[1] = (uint8_t)(bits >> 8);
}

/** \brief Fixture clock advances only when the test schedules a poll. */
static nx_time_us_t clock_read(void* context) {
    return ((fixture_t*)context)->now;
}

/** \brief Execute full-duplex BMP280 register traffic in one CS interval. */
static nx_result_t exchange(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline) {
    fixture_t* fixture = context;
    CHECK(!fixture->borrowed && length >= 2 && tx != NULL);
    CHECK(deadline >= fixture->now);
    fixture->borrowed = true;
    ++fixture->calls;
    if (fixture->failure != NX_SUCCESS) {
        fixture->borrowed = false;
        return fixture->failure;
    }
    if ((tx[0] & 0x80u) != 0) {
        CHECK(rx != NULL);
        rx[0] = 0xff;
        for (size_t i = 1; i < length; ++i) {
            rx[i] = fixture->registers[(uint8_t)(tx[0] + i - 1)];
        }
    } else {
        CHECK(length == 2);
        fixture->registers[tx[0] | 0x80u] = tx[1];
        ++fixture->writes;
    }
    fixture->borrowed = false;
    return NX_SUCCESS;
}

/** \brief Assemble register facts without claiming a board connection. */
static void initialize(fixture_t* fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->registers[0xd0] = 0x58;
    const int32_t words[12] = {27504, 26435, -1000, 36477, -10685, 3024,
                               2855,  140,   -7,    15500, -14600, 6000};
    for (size_t i = 0; i < 12; ++i) {
        put16(&fixture->registers[0x88 + i * 2], words[i]);
    }
    uint32_t raw_pressure = 415148;
    uint32_t raw_temperature = 519888;
    for (size_t i = 0; i < 3; ++i) {
        unsigned shift = (unsigned)(16u - i * 8u);
        fixture->registers[0xf7 + i] = (uint8_t)((raw_pressure << 4) >> shift);
        fixture->registers[0xfa + i] =
            (uint8_t)((raw_temperature << 4) >> shift);
    }
}

/**
 * \brief           Validate Bosch reference output and malformed coefficient
 *                  rejection.
 */
static void compensation(void) {
    nx_bmp280_sample_t sample = {-9999, 123};
    CHECK(nx_bmp280_compensate(&s_calibration, 519888, 415148, &sample) ==
          NX_SUCCESS);
    CHECK(sample.centi_celsius == 2508 && sample.pressure_pa == 100653);
    nx_bmp280_sample_t previous = sample;
    CHECK(nx_bmp280_compensate(&s_calibration, 0x80000, 415148, &sample) ==
          NX_ERROR_IO);
    CHECK(memcmp(&sample, &previous, sizeof(sample)) == 0);
    CHECK(nx_bmp280_compensate(&s_calibration, 0x100000, 415148, &sample) ==
          NX_ERROR_INVALID);
    nx_bmp280_calibration_t broken = s_calibration;
    broken.p1 = 0;
    CHECK(nx_bmp280_compensate(&broken, 519888, 415148, &sample) ==
          NX_ERROR_INVALID);
    uint32_t seed = 7;
    for (unsigned i = 0; i < 10000; ++i) {
        uint8_t raw[24];
        for (size_t j = 0; j < sizeof(raw); ++j) {
            seed = seed * 1664525u + 1013904223u;
            raw[j] = (uint8_t)(seed >> 24);
        }
        if (nx_bmp280_parse_calibration(raw, sizeof(raw), &broken) ==
            NX_SUCCESS) {
            (void)nx_bmp280_compensate(&broken, seed & 0xfffffu,
                                       (seed >> 4) & 0xfffffu, &sample);
        }
    }
}

/**
 * \brief           Exercise wrong ID/NVM errors, no stale conversion and
 *                  timeout retry.
 */
static void lifecycle(void) {
    fixture_t fixture;
    initialize(&fixture);
    nx_bmp280_transport_port_t transport = {
        &fixture, exchange, {clock_read, &fixture}};
    nx_bmp280_t device = {0};
    fixture.registers[0xd0] = 0x60;
    CHECK(nx_bmp280_init(&device, transport, 100000) == NX_ERROR_UNSUPPORTED);
    CHECK(!device.initialized && !fixture.borrowed);
    fixture.registers[0xd0] = 0x58;
    fixture.registers[0xf3] = 1;
    CHECK(nx_bmp280_init(&device, transport, 100000) == NX_ERROR_BUSY);
    fixture.registers[0xf3] = 0;
    fixture.failure = NX_ERROR_IO;
    CHECK(nx_bmp280_init(&device, transport, 100000) == NX_ERROR_IO);
    fixture.failure = NX_SUCCESS;
    CHECK(nx_bmp280_init(&device, transport, 100000) == NX_SUCCESS);
    CHECK(device.calibration.p2 == -10685 && fixture.writes == 1);
    CHECK(nx_bmp280_begin(&device, 100000) == NX_SUCCESS);
    CHECK(fixture.registers[0xf4] == 0x25);
    nx_bmp280_sample_t sample;
    unsigned calls = fixture.calls;
    CHECK(nx_bmp280_poll(&device, &sample) == NX_ERROR_BUSY);
    CHECK(calls == fixture.calls);
    fixture.now = 8000;
    fixture.registers[0xf3] = 8;
    CHECK(nx_bmp280_poll(&device, &sample) == NX_ERROR_BUSY);
    fixture.registers[0xf3] = 0;
    CHECK(nx_bmp280_poll(&device, &sample) == NX_SUCCESS);
    CHECK(sample.centi_celsius == 2508 && sample.pressure_pa == 100653);
    CHECK(nx_bmp280_poll(&device, &sample) == NX_ERROR_STATE);
    CHECK(nx_bmp280_begin(&device, 9000) == NX_SUCCESS);
    fixture.now = 9000;
    CHECK(nx_bmp280_poll(&device, &sample) == NX_ERROR_TIMEOUT);
    fixture.registers[0xf3] = 8;
    CHECK(nx_bmp280_begin(&device, 20000) == NX_ERROR_BUSY);
    fixture.registers[0xf3] = 0;
    CHECK(nx_bmp280_begin(&device, 20000) == NX_SUCCESS);
    fixture.now = 18000;
    fixture.failure = NX_ERROR_IO;
    CHECK(nx_bmp280_poll(&device, &sample) == NX_ERROR_IO);
    CHECK(!device.measuring && !fixture.borrowed);
    nx_bmp280_stop(&device);
    CHECK(nx_bmp280_begin(&device, 30000) == NX_ERROR_STATE);
}

/** \brief Run actual BMP280 parsing/conversion/lifetime behavior. */
int main(void) {
    compensation();
    lifecycle();
    puts("BMP280 chip ID, calibration, Bosch vector, timing and retry passed");
    return 0;
}
