/**
 * \file            i2c_transport.c
 * \brief           Settled LSM6DSOX repeated-START register transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/lsm6dsox_i2c.h"
#include <string.h>

static nx_result_t read_register(void* context, uint8_t reg, uint8_t* data,
                                 size_t bytes, nx_time_us_t deadline) {
    if (data == NULL || bytes == 0 || bytes > 6) {
        return NX_ERROR_INVALID;
    }
    nx_i2c_message_t messages[2] = {nx_i2c_message_write(&reg, 1),
                                    nx_i2c_message_read(data, bytes)};
    size_t transferred = 0;
    const nx_result_t result = nx_i2c_endpoint_transaction(
        context, messages, 2, deadline, &transferred);
    return result == NX_SUCCESS && transferred != bytes + 1 ? NX_ERROR_IO
                                                            : result;
}

static nx_result_t write_register(void* context, uint8_t reg,
                                  const uint8_t* data, size_t bytes,
                                  nx_time_us_t deadline) {
    if (data == NULL || bytes == 0 || bytes > 6) {
        return NX_ERROR_INVALID;
    }
    uint8_t buffer[7] = {reg};
    memcpy(buffer + 1, data, bytes);
    nx_i2c_message_t message = nx_i2c_message_write(buffer, bytes + 1);
    size_t transferred = 0;
    const nx_result_t result = nx_i2c_endpoint_transaction(
        context, &message, 1, deadline, &transferred);
    return result == NX_SUCCESS && transferred != bytes + 1 ? NX_ERROR_IO
                                                            : result;
}

nx_lsm6dsox_transport_port_t
nx_lsm6dsox_i2c_transport(const nx_i2c_endpoint_t* endpoint, nx_clock_t clock) {
    if (endpoint == NULL || endpoint->context == NULL ||
        endpoint->ops == NULL || endpoint->ops->transaction == NULL ||
        clock.read == NULL) {
        return (nx_lsm6dsox_transport_port_t){0};
    }
    const nx_lsm6dsox_transport_port_t port = {(void*)endpoint, read_register,
                                               write_register, clock};
    return port;
}
