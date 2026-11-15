/**
 * \file            spi_transport.c
 * \brief           BMP280 transport over one explicit polling SPI endpoint
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bmp280_spi.h"

/** \brief Require exact complete bytes from the synchronous settled IO port. */
static nx_result_t exchange(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline) {
    size_t transferred = 0;
    nx_result_t result = nx_spi_endpoint_transfer(context, tx, rx, length,
                                                  deadline, &transferred);
    return result == NX_SUCCESS && transferred != length ? NX_ERROR_IO : result;
}

/** \brief Bind a fixed endpoint without discovery, construction or startup. */
nx_bmp280_transport_port_t
nx_bmp280_spi_transport(const nx_spi_endpoint_t* endpoint, nx_clock_t clock) {
    nx_bmp280_transport_port_t port = {(void*)endpoint, exchange, clock};
    return port;
}
