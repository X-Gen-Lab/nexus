/**
 * \file            spi_owner.c
 * \brief           Real polling SPI executor with explicit settlement reporting
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/spi_owner.h"

/** \brief Execute synchronously, preserving BUSY as not-started queue state. */
static nx_result_t start(void* context, void* operation,
                         nx_time_us_t deadline) {
    nx_spi_owner_executor_t* executor = context;
    nx_spi_owner_operation_t* transfer = operation;
    if (executor->pending) {
        return NX_ERROR_BUSY;
    }
    if (transfer->endpoint == NULL || transfer->length == 0 ||
        (transfer->tx == NULL && transfer->rx == NULL)) {
        return NX_ERROR_INVALID;
    }
    size_t transferred = 0;
    nx_result_t result =
        nx_spi_endpoint_transfer(transfer->endpoint, transfer->tx, transfer->rx,
                                 transfer->length, deadline, &transferred);
    if (result == NX_ERROR_BUSY) {
        return result;
    }
    executor->result = result;
    executor->transferred = transferred;
    executor->pending = true;
    return NX_SUCCESS;
}

/**
 * \brief           Report already-drained polling completion without touching
 *                  payloads.
 */
static nx_result_t service(void* context, nx_result_t* result,
                           size_t* transferred) {
    nx_spi_owner_executor_t* executor = context;
    if (!executor->pending) {
        return NX_ERROR_STATE;
    }
    *result = executor->result;
    *transferred = executor->transferred;
    executor->pending = false;
    return NX_SUCCESS;
}

/**
 * \brief           Polling has already drained; late cancel cannot rewrite its
 *                  result.
 */
static nx_result_t cancel(void* context) {
    (void)context;
    return NX_SUCCESS;
}

/** \brief Select only the real polling provider, without a default worker. */
nx_owner_executor_port_t
nx_spi_owner_executor_port(nx_spi_owner_executor_t* executor) {
    nx_owner_executor_port_t port = {executor, start, service, cancel, true};
    return port;
}
