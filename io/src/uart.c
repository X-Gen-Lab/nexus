/**
 * \file            uart.c
 *
 * \brief           Provider-independent caller-owned TX preparation.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/uart.h"

/** \brief Prepare only unborrowed metadata and retain no payload reference. */
nx_result_t nx_uart_tx_prepare(nx_uart_tx_request_t* request,
                               const uint8_t* data, size_t length,
                               nx_time_us_t deadline) {
    if (request == NULL || data == NULL || length == 0) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    request->data = data;
    request->length = length;
    return NX_SUCCESS;
}
