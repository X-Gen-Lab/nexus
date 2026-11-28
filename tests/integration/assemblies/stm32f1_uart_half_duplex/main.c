/**
 * \file            main.c
 * \brief           Consume the USART2 single-wire port using the typed factory.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_uart_port_t* link = nx_factory_uart_ref(NX_UART_REF_LINK);
    const uint8_t byte = 0xa5U;
    nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    const bool rejected =
        nx_uart_tx_prepare(&request, &byte, 1U, 0U) == NX_SUCCESS &&
        nx_uart_port_submit(link, &request) == NX_ERROR_TIMEOUT &&
        nx_request_state(&request.base) == NX_REQUEST_READY;
    const nx_result_t parked = nx_uart_port_stop(link);
    const nx_result_t stopped = nx_platform_stop();
    return rejected && parked == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
