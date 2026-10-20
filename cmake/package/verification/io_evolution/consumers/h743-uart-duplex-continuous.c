/**
 * \file            h743-uart-duplex-continuous.c
 * \brief           Public continuous RX and finite TX DMA loan consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

_Alignas(32) static uint8_t s_buffers[4][64];
_Alignas(32) static uint8_t s_transmit_bytes[64] = "Nexus UART DMA duplex\r\n";
static nx_stream_slot_t s_slots[4] = {
    {.data = s_buffers[0], .capacity = sizeof(s_buffers[0])},
    {.data = s_buffers[1], .capacity = sizeof(s_buffers[1])},
    {.data = s_buffers[2], .capacity = sizeof(s_buffers[2])},
    {.data = s_buffers[3], .capacity = sizeof(s_buffers[3])}};
static nx_stream_t s_stream;
static nx_uart_tx_request_t s_transmit;

/** \brief Failed drain never permits reclamation of static DMA loans. */
static void retain_storage(void) {
    for (;;) {
    }
}

/** \brief Join RX consumer loans and TX completion before outer withdrawal. */
static int close_platform(const nx_uart_port_t* uart, int result) {
    if (uart != NULL) {
        (void)nx_uart_port_cancel(uart, &s_transmit);
        (void)nx_uart_port_rx_stop(uart);
    }
    for (unsigned turn = 0U; turn < 256U; ++turn) {
        nx_stream_block_t block;
        if (uart != NULL) {
            nx_uart_port_service(uart);
        }
        while (nx_stream_acquire(&s_stream, &block) == NX_SUCCESS) {
            if (nx_stream_release(&s_stream, &block) != NX_SUCCESS) {
                retain_storage();
            }
        }
        if (nx_platform_stop() == NX_SUCCESS) {
            return result;
        }
    }
    retain_storage();
    return result;
}

/** \brief Link exact generated C11 faces; no physical qualification follows. */
int main(void) {
    nx_request_initialize(&s_transmit.base);
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS) {
        return close_platform(NULL, 1);
    }
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    if (uart == NULL ||
        nx_stream_initialize(&s_stream, s_slots, 4U) != NX_SUCCESS ||
        nx_uart_port_rx_start(uart, &s_stream) != NX_SUCCESS) {
        return close_platform(uart, 2);
    }
    if (nx_uart_tx_prepare(
            &s_transmit, s_transmit_bytes, sizeof(s_transmit_bytes),
            nx_deadline_after(nx_time_now_us(), 10000U)) != NX_SUCCESS ||
        nx_uart_port_submit(uart, &s_transmit) != NX_SUCCESS) {
        return close_platform(uart, 3);
    }
    (void)nx_uart_port_cancel(uart, &s_transmit);
    for (unsigned turn = 0U; turn < 128U; ++turn) {
        nx_uart_port_service(uart);
        if (nx_request_state(&s_transmit.base) == NX_REQUEST_SETTLED) {
            return close_platform(uart, 0);
        }
    }
    return close_platform(uart, 4);
}
