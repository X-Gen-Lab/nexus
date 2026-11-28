/**
 * \file            main.c
 * \brief           Use the Cortex-M3 word port through the static typed
 * factory. \author          Nexus Team \version         1.0.0 \date 2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_uart_word_port_t* link =
        nx_factory_uart_word_ref(NX_UART_WORD_REF_LINK);
    const uint16_t words[] = {0x103U, 0x1ffU};
    nx_uart_word_tx_request_t request;
    nx_request_initialize(&request.base);
    const bool rejected_without_tx =
        nx_uart_word_tx_prepare(&request, words, 2U, 0U) == NX_SUCCESS &&
        nx_uart_word_port_submit(link, &request) == NX_ERROR_TIMEOUT &&
        nx_request_state(&request.base) == NX_REQUEST_READY;
    const nx_result_t parked = nx_uart_word_port_stop(link);
    const nx_result_t stopped = nx_platform_stop();
    return rejected_without_tx && parked == NX_SUCCESS && stopped == NX_SUCCESS
               ? 0
               : 2;
}
