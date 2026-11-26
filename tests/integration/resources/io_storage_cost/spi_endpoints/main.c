/**
 * \file            main.c
 * \brief           One-controller endpoint allocation comparison consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus/io/spi.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

#if COST_ENDPOINTS != 0
static volatile unsigned s_endpoint_count = COST_ENDPOINTS;
#endif
static volatile nx_result_t s_result;

/** \brief Consume selected topology and endpoints with one polling loop. */
int main(void) {
#if COST_ENDPOINTS != 0
    const nx_spi_port_t* controller = nx_factory_spi_ref(NX_SPI_REF_SPI0);
    const nx_spi_endpoint_t* endpoints[] = {
        nx_factory_spi_device_ref(NX_SPI_DEVICE_REF_SENSOR0),
#if COST_ENDPOINTS >= 2
        nx_factory_spi_device_ref(NX_SPI_DEVICE_REF_SENSOR1),
#else
        NULL,
#endif
#if COST_ENDPOINTS == 4
        nx_factory_spi_device_ref(NX_SPI_DEVICE_REF_SENSOR2),
        nx_factory_spi_device_ref(NX_SPI_DEVICE_REF_SENSOR3),
#endif
    };
#endif
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
#if COST_ENDPOINTS != 0
    for (unsigned index = 0U; index < s_endpoint_count &&
                              index < sizeof(endpoints) / sizeof(endpoints[0]);
         ++index) {
        if (!nx_spi_endpoint_on_port(endpoints[index], controller)) {
            s_result = NX_ERROR_STATE;
            break;
        }
        uint8_t tx = 0x9fU;
        uint8_t rx = 0U;
        size_t transferred = 0U;
        s_result = nx_spi_endpoint_transfer(
            endpoints[index], &tx, &rx, 1U,
            nx_deadline_after(nx_time_now_us(), 100000U), &transferred);
    }
#endif
    s_result = nx_platform_stop();
    for (;;) {
    }
}
