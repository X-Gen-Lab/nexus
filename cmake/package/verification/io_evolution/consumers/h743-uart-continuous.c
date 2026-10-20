/**
 * \file            h743-uart-continuous.c
 * \brief           Public continuous RX and independent finite IRQ TX consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

_Alignas(32) static uint8_t s_buffers[4][64];
static nx_stream_slot_t s_slots[4] = {
    {.data = s_buffers[0], .capacity = sizeof(s_buffers[0])},
    {.data = s_buffers[1], .capacity = sizeof(s_buffers[1])},
    {.data = s_buffers[2], .capacity = sizeof(s_buffers[2])},
    {.data = s_buffers[3], .capacity = sizeof(s_buffers[3])}};
static nx_stream_t s_stream;
static nx_uart_tx_request_t s_transmit;
static const uint8_t s_message[] = "Nexus continuous RX\r\n";

/** \brief Retained hardware/consumer loans forbid reclaiming static storage. */
static void retain_storage(void) {
    for (;;) {
    }
}

/** \brief Drain public consumer loans before retrying the outer owner stop. */
static int close_platform(int result) {
    for (unsigned turn = 0U; turn < 256U; ++turn) {
        nx_stream_block_t block;
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

/** \brief Link normally generated C11 faces; physical execution is separate. */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    if (uart == NULL ||
        nx_stream_initialize(&s_stream, s_slots, 4U) != NX_SUCCESS ||
        nx_uart_port_rx_start(uart, &s_stream) != NX_SUCCESS) {
        return close_platform(2);
    }
    nx_request_initialize(&s_transmit.base);
    if (nx_uart_tx_prepare(&s_transmit, s_message, sizeof(s_message) - 1U,
                           nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_uart_port_submit(uart, &s_transmit) != NX_SUCCESS) {
        return close_platform(3);
    }
    (void)nx_uart_port_cancel(uart, &s_transmit);
    for (unsigned turn = 0U; turn < 128U; ++turn) {
        nx_uart_port_service(uart);
    }
    (void)nx_uart_port_rx_stop(uart);
    return close_platform(0);
}
