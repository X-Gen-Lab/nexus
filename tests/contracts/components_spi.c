/**
 * \file            components_spi.c
 * \brief           Real common IO adapters for BMP280 and explicit SPI owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bmp280_spi.h"
#include "nexus/components/spi_owner.h"
#include "nexus/io/native/model.h"
#include "nexus/os/native.h"
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

/**
 * \brief           Explicit short multi-producer guard, never held over SPI
 *                  execution.
 */
static uintptr_t enter(void* context) {
    CHECK(pthread_mutex_lock(context) == 0);
    return 0;
}

/** \brief Release exact producer metadata guard. */
static void leave(void* context, uintptr_t state) {
    (void)state;
    CHECK(pthread_mutex_unlock(context) == 0);
}

/** \brief Read IO's selected clock domain directly. */
static uint64_t clock_read(void* context) {
    (void)context;
    return nx_time_now_us();
}

/**
 * \brief           Populate a SPI register fixture using its 7-bit wire address
 *                  mapping.
 */
static void bmp280_integration(void) {
    uint8_t registers[128] = {0};
    registers[0x50] = 0x58;
    const int32_t calibration[] = {27504, 26435, -1000, 36477, -10685, 3024,
                                   2855,  140,   -7,    15500, -14600, 6000};
    for (size_t i = 0; i < 12; ++i) {
        uint16_t word = (uint16_t)calibration[i];
        registers[0x08 + i * 2] = (uint8_t)word;
        registers[0x09 + i * 2] = (uint8_t)(word >> 8);
    }
    const uint32_t raw[] = {415148, 519888};
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            unsigned shift = (unsigned)(16u - j * 8u);
            registers[0x77 + i * 3 + j] = (uint8_t)((raw[i] << 4) >> shift);
        }
    }
    CHECK(nx_native_clock_configure(true, 0) == NX_SUCCESS);
    CHECK(nx_native_spi_configure(registers, sizeof(registers)) == NX_SUCCESS);
    nx_clock_t clock = {clock_read, NULL};
    nx_bmp280_t device = {0};
    CHECK(nx_bmp280_init(&device, nx_bmp280_spi_transport(nx_native_spi, clock),
                         100000) == NX_SUCCESS);
    CHECK(nx_bmp280_begin(&device, 100000) == NX_SUCCESS);
    CHECK(nx_native_clock_advance(8000) == NX_SUCCESS);
    nx_bmp280_sample_t sample;
    CHECK(nx_bmp280_poll(&device, &sample) == NX_SUCCESS);
    CHECK(sample.centi_celsius == 2508 && sample.pressure_pa == 100653);
    CHECK(!nx_native_spi_cs_active());
    nx_native_spi_fault(1, false);
    CHECK(nx_bmp280_begin(&device, 100000) == NX_ERROR_IO);
    CHECK(!device.measuring && !nx_native_spi_cs_active());
    nx_bmp280_stop(&device);
}

/**
 * \brief           One polling owner uses actual partial/error and pre-start
 *                  BUSY paths.
 */
static void spi_owner_integration(void) {
    CHECK(nx_native_spi_configure(NULL, 0) == NX_SUCCESS);
    pthread_mutex_t mutex;
    CHECK(pthread_mutex_init(&mutex, NULL) == 0);
    nx_owner_guard_port_t guard = {&mutex, enter, leave};
    nx_spi_owner_executor_t executor = {0};
    nx_owner_slot_t slots[1];
    nx_bus_owner_t owner;
    nx_clock_t clock = {clock_read, NULL};
    CHECK(nx_bus_owner_init(&owner, slots, 1, guard,
                            nx_spi_owner_executor_port(&executor), clock,
                            NULL) == NX_SUCCESS);
    uint8_t tx[4] = {1, 2, 3, 4};
    uint8_t rx[4] = {0};
    nx_spi_owner_operation_t operation = {nx_native_spi, tx, rx, sizeof(tx)};
    nx_request_t request;
    nx_request_initialize(&request);
    CHECK(nx_request_prepare(&request, 100000) == NX_SUCCESS);
    nx_owner_ticket_t ticket;
    CHECK(nx_bus_owner_submit(&owner, &request, &operation, NULL, &ticket) ==
          NX_SUCCESS);
    nx_native_spi_fault(SIZE_MAX, true);
    CHECK(nx_bus_owner_service(&owner) == NX_ERROR_BUSY);
    CHECK(nx_request_state(&request) == NX_REQUEST_QUEUED &&
          !nx_native_spi_cs_active());
    nx_native_spi_fault(2, false);
    CHECK(nx_bus_owner_service(&owner) == NX_SUCCESS);
    CHECK(nx_request_state(&request) == NX_REQUEST_ACTIVE);
    CHECK(!nx_native_spi_cs_active());
    CHECK(nx_bus_owner_service(&owner) == NX_SUCCESS);
    nx_result_t result;
    size_t transferred;
    CHECK(nx_request_result(&request, &result, &transferred) == NX_SUCCESS);
    CHECK(result == NX_ERROR_IO && transferred == 2);
    CHECK(rx[0] == 1 && rx[1] == 2 && rx[2] == 0);
    CHECK(nx_bus_owner_idle(&owner));
    CHECK(nx_bus_owner_cancel(&owner, ticket) == NX_ERROR_STATE);

    /* Wire completion precedes publication: late controls cannot undo it. */
    nx_native_spi_fault(SIZE_MAX, false);
    CHECK(nx_request_prepare(&request, nx_time_now_us() + 10) == NX_SUCCESS);
    CHECK(nx_bus_owner_submit(&owner, &request, &operation, NULL, &ticket) ==
          NX_SUCCESS);
    CHECK(nx_bus_owner_service(&owner) == NX_SUCCESS);
    CHECK(nx_native_clock_advance(10) == NX_SUCCESS);
    CHECK(nx_bus_owner_cancel(&owner, ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&owner) == NX_SUCCESS);
    CHECK(nx_request_result(&request, &result, &transferred) == NX_SUCCESS);
    CHECK(result == NX_SUCCESS && transferred == sizeof(tx));
    CHECK(!memcmp(tx, rx, sizeof(tx)));
    CHECK(pthread_mutex_destroy(&mutex) == 0);
}

/**
 * \brief           Run actual provider/adapter/core vertical paths, with no
 *                  board claim.
 */
int main(void) {
    bmp280_integration();
    spi_owner_integration();
    puts("BMP280 and SPI owner actual IO adapter integration passed");
    return 0;
}
