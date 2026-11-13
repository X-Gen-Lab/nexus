/**
 * \file            i2c_transport.c
 * \brief           Settled TMP117 pointer/read and register-write transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/tmp117_i2c.h"

/** \brief Only the synchronous public endpoint may receive local arrays. */
static nx_result_t exchange(void* context, const uint8_t* tx, size_t tx_bytes,
                            uint8_t* rx, size_t rx_bytes,
                            nx_time_us_t deadline) {
    if (context == NULL || tx == NULL || deadline == NX_DEADLINE_NEVER ||
        !((tx_bytes == 1U && rx_bytes == 2U && rx != NULL) ||
          (tx_bytes == 3U && rx_bytes == 0U && rx == NULL))) {
        return NX_ERROR_INVALID;
    }
    /* Write data are immutable under the public I2C message contract. */
    nx_i2c_message_t messages[2] = {nx_i2c_message_write(tx, tx_bytes),
                                    nx_i2c_message_read(rx, rx_bytes)};
    size_t count = rx_bytes == 0U ? 1U : 2U;
    size_t transferred = 0U;
    nx_result_t result = nx_i2c_endpoint_transaction(context, messages, count,
                                                     deadline, &transferred);
    return result == NX_SUCCESS && transferred != tx_bytes + rx_bytes
               ? NX_ERROR_IO
               : result;
}

nx_tmp117_transport_port_t
nx_tmp117_i2c_transport(const nx_i2c_endpoint_t* endpoint, nx_clock_t clock) {
    if (endpoint == NULL || endpoint->ops == NULL ||
        endpoint->context == NULL || endpoint->ops->transaction == NULL ||
        clock.read == NULL) {
        return (nx_tmp117_transport_port_t){0};
    }
    return (nx_tmp117_transport_port_t){(void*)endpoint, exchange, clock};
}
