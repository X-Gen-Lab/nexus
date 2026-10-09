/**
 * \file            uart_owner.c
 * \brief           IRQ UART bridge preserving independent request publication
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/uart_owner.h"

/**
 * \brief           Clear private payload references before giving the outer
 *                  loan back.
 */
static void detach(nx_uart_owner_executor_t* executor) {
    executor->inner.data = NULL;
    executor->inner.length = 0;
    executor->pending = false;
}

/** \brief Directly admit only the private request; the outer remains owned. */
static nx_result_t start(void* context, void* operation,
                         nx_time_us_t deadline) {
    nx_uart_owner_executor_t* executor = context;
    nx_uart_owner_operation_t* transfer = operation;
    if (executor->pending) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_uart_tx_prepare(&executor->inner, transfer->data,
                                            transfer->length, deadline);
    if (result == NX_SUCCESS) {
        result = nx_uart_port_submit(executor->port, &executor->inner);
    }
    if (result != NX_SUCCESS) {
        detach(executor);
        return result;
    }
    executor->pending = true;
    return NX_SUCCESS;
}

/**
 * \brief           Collect only after provider publication; quarantine retains
 *                  storage.
 */
static nx_result_t service(void* context, nx_result_t* result,
                           size_t* transferred) {
    nx_uart_owner_executor_t* executor = context;
    if (!executor->pending) {
        return NX_ERROR_STATE;
    }
    nx_uart_port_service(executor->port);
    nx_request_state_t state = nx_request_state(&executor->inner.base);
    if (state != NX_REQUEST_SETTLED) {
        return state == NX_REQUEST_QUARANTINED ? NX_ERROR_IO : NX_ERROR_BUSY;
    }
    nx_result_t collected =
        nx_request_result(&executor->inner.base, result, transferred);
    if (collected != NX_SUCCESS) {
        return collected;
    }
    detach(executor);
    return NX_SUCCESS;
}

/**
 * \brief           Preserve an observed TC or deadline before issuing
 *                  best-effort abort.
 */
static nx_result_t cancel(void* context) {
    nx_uart_owner_executor_t* executor = context;
    if (!executor->pending) {
        return NX_ERROR_STATE;
    }
    nx_uart_port_service(executor->port);
    if (nx_request_state(&executor->inner.base) == NX_REQUEST_SETTLED) {
        return NX_SUCCESS;
    }
    return nx_uart_port_cancel(executor->port, &executor->inner);
}

/** \brief Initialize exactly one private request, without opening hardware. */
nx_owner_executor_port_t
nx_uart_owner_executor_port(nx_uart_owner_executor_t* executor,
                            nx_uart_port_t* uart) {
    if (executor == NULL || uart == NULL) {
        return (nx_owner_executor_port_t){0};
    }
    *executor = (nx_uart_owner_executor_t){0};
    executor->port = uart;
    nx_request_initialize(&executor->inner.base);
    return (nx_owner_executor_port_t){executor, start, service, cancel, true};
}
