/**
 * \file            spi_async_owner.c
 * \brief           Async SPI bridge preserves bus identity and final drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/spi_async_owner.h"

/** \brief Drop all private payload references before releasing the outer loan.
 */
static void detach(nx_spi_async_owner_executor_t* executor) {
    executor->inner.tx = NULL;
    executor->inner.rx = NULL;
    executor->inner.length = 0;
    executor->pending = false;
}

/** \brief Reject a foreign bus before preparation, admission or hardware work.
 */
static nx_result_t start(void* context, void* operation,
                         nx_time_us_t deadline) {
    nx_spi_async_owner_executor_t* executor = context;
    nx_spi_owner_operation_t* transfer = operation;
    if (executor->pending) {
        return NX_ERROR_BUSY;
    }
    if (transfer == NULL ||
        !nx_spi_endpoint_on_port(transfer->endpoint, executor->port)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        nx_spi_request_prepare(&executor->inner, transfer->tx, transfer->rx,
                               transfer->length, deadline);
    if (result == NX_SUCCESS) {
        result = nx_spi_endpoint_submit(transfer->endpoint, &executor->inner);
    }
    if (result != NX_SUCCESS) {
        detach(executor);
        return result;
    }
    executor->pending = true;
    return NX_SUCCESS;
}

/** \brief Detach only after acquire-observed private provider settlement. */
static nx_result_t service(void* context, nx_result_t* result,
                           size_t* transferred) {
    nx_spi_async_owner_executor_t* executor = context;
    if (!executor->pending) {
        return NX_ERROR_STATE;
    }
    nx_spi_port_service(executor->port);
    nx_request_state_t state = nx_request_state(&executor->inner.base);
    if (state != NX_REQUEST_SETTLED) {
        return state == NX_REQUEST_QUARANTINED ? NX_ERROR_IO : NX_ERROR_BUSY;
    }
    nx_result_t collected =
        nx_request_result(&executor->inner.base, result, transferred);
    if (collected == NX_SUCCESS) {
        detach(executor);
    }
    return collected;
}

/** \brief Observe an earlier wire/deadline fact before requesting cancellation.
 */
static nx_result_t cancel(void* context) {
    nx_spi_async_owner_executor_t* executor = context;
    if (!executor->pending) {
        return NX_ERROR_STATE;
    }
    nx_spi_port_service(executor->port);
    return nx_request_state(&executor->inner.base) == NX_REQUEST_SETTLED
               ? NX_SUCCESS
               : nx_spi_port_cancel(executor->port, &executor->inner);
}

/** \brief Assemble exact metadata without allocating or opening hardware. */
nx_owner_executor_port_t
nx_spi_async_owner_executor_port(nx_spi_async_owner_executor_t* executor,
                                 const nx_spi_port_t* port) {
    if (executor == NULL || port == NULL || port->ops == NULL ||
        port->context == NULL || port->ops->cancel == NULL ||
        port->ops->service == NULL) {
        return (nx_owner_executor_port_t){0};
    }
    *executor = (nx_spi_async_owner_executor_t){.port = port};
    nx_request_initialize(&executor->inner.base);
    return (nx_owner_executor_port_t){executor, start, service, cancel, true};
}
