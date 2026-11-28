/**
 * \file            main.c
 * \brief           Resolve and independently operate both F103RB byte ports.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static bool rejects_expired(const nx_uart_port_t* port) {
    const uint8_t byte = 0x55U;
    nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    return nx_uart_tx_prepare(&request, &byte, 1U, 0U) == NX_SUCCESS &&
           nx_uart_port_submit(port, &request) == NX_ERROR_TIMEOUT &&
           nx_request_state(&request.base) == NX_REQUEST_READY;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_uart_port_t* external = nx_factory_uart_ref(NX_UART_REF_EXTERNAL);
    const nx_uart_port_t* console = nx_factory_uart_ref(NX_UART_REF_CONSOLE);
    const bool isolated = external != console && rejects_expired(external) &&
                          nx_uart_port_stop(external) == NX_SUCCESS &&
                          rejects_expired(console);
    const nx_result_t stopped = nx_platform_stop();
    return isolated && stopped == NX_SUCCESS ? 0 : 2;
}
