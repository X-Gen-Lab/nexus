/**
 * \file            uart_format.c
 * \brief           Shared UART format validation independent of transfer mode
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/uart_format.h"

/** \brief Unknown encodings never silently select a different format. */
nx_result_t nx_uart_format_validate(nx_uart_format_t format) {
    if ((format.parity != NX_UART_PARITY_NONE &&
         format.parity != NX_UART_PARITY_EVEN &&
         format.parity != NX_UART_PARITY_ODD) ||
        (format.stop != NX_UART_STOP_1 && format.stop != NX_UART_STOP_2) ||
        (format.flow != NX_UART_FLOW_NONE && format.flow != NX_UART_FLOW_RTS &&
         format.flow != NX_UART_FLOW_CTS &&
         format.flow != NX_UART_FLOW_RTS_CTS) ||
        format.data_bits < 5U || format.data_bits > 9U) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
