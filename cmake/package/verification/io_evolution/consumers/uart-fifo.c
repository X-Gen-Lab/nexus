/**
 * \file            uart-fifo.c
 * \brief           Public installed-SDK selected FIFO byte ownership linking
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_uart_tx_request_t s_request;
static const uint8_t s_payload[] = {0x12U, 0x34U, 0x56U, 0x78U};
static uint8_t s_received[8];
static size_t s_count;

/** \brief Keep accepted storage alive if final withdrawal cannot be proved. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Close the provider plus its outer pins, clock and vector leases. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}

/** \brief Reuse public byte APIs; linking does not execute or measure FIFO. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    nx_request_initialize(&s_request.base);
    if (uart == NULL ||
        nx_uart_tx_prepare(&s_request, s_payload, sizeof(s_payload),
                           nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_uart_port_submit(uart, &s_request) != NX_SUCCESS) {
        return close_platform(2);
    }
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_uart_port_cancel(uart, &s_request);
    }
    for (unsigned i = 0U;
         i != 64U && nx_request_state(&s_request.base) != NX_REQUEST_SETTLED;
         ++i) {
        nx_uart_port_service(uart);
    }
    (void)nx_uart_port_read_bytes(uart, s_received, sizeof(s_received),
                                  &s_count);
    (void)nx_uart_port_stop(uart);
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
    }
    return close_platform(0);
}
