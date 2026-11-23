/**
 * \file            spi_transport.c
 * \brief           Settled LSM6DSOX whole-CS register transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/lsm6dsox_spi.h"
#include <string.h>

static nx_result_t read_register(void* context, uint8_t reg, uint8_t* data,
                                 size_t bytes, nx_time_us_t deadline) {
    if (data == NULL || bytes == 0 || bytes > 6) {
        return NX_ERROR_INVALID;
    }
    uint8_t tx[7] = {(uint8_t)(reg | 0x80U)};
    uint8_t rx[7];
    size_t transferred = 0;
    const nx_result_t result = nx_spi_endpoint_transfer(
        context, tx, rx, bytes + 1, deadline, &transferred);
    if (result != NX_SUCCESS || transferred != bytes + 1) {
        return result == NX_SUCCESS ? NX_ERROR_IO : result;
    }
    memcpy(data, rx + 1, bytes);
    return NX_SUCCESS;
}

static nx_result_t write_register(void* context, uint8_t reg,
                                  const uint8_t* data, size_t bytes,
                                  nx_time_us_t deadline) {
    if (data == NULL || bytes == 0 || bytes > 6) {
        return NX_ERROR_INVALID;
    }
    uint8_t tx[7] = {(uint8_t)(reg & 0x7fU)};
    memcpy(tx + 1, data, bytes);
    size_t transferred = 0;
    const nx_result_t result = nx_spi_endpoint_transfer(
        context, tx, NULL, bytes + 1, deadline, &transferred);
    return result == NX_SUCCESS && transferred != bytes + 1 ? NX_ERROR_IO
                                                            : result;
}

nx_lsm6dsox_transport_port_t
nx_lsm6dsox_spi_transport(const nx_spi_endpoint_t* endpoint, nx_clock_t clock) {
    if (endpoint == NULL || endpoint->context == NULL ||
        endpoint->ops == NULL || endpoint->ops->transfer == NULL ||
        clock.read == NULL) {
        return (nx_lsm6dsox_transport_port_t){0};
    }
    const nx_lsm6dsox_transport_port_t port = {(void*)endpoint, read_register,
                                               write_register, clock};
    return port;
}
