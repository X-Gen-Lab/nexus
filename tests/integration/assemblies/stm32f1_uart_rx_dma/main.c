/**
 * \file            main.c
 * \brief           Link the selected USART2 finite DMA RX face and stream
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_stream_t stream;
static nx_stream_slot_t slots[2];
static uint8_t blocks[2][32];

/** \brief Start and stop with linker-owned DMA-reachable SRAM. */
int main(void) {
    const nx_platform_start_result_t platform = nx_platform_start();
    if (platform.primary != NX_SUCCESS || platform.cleanup != NX_SUCCESS)
        return 1;
    const nx_uart_port_t* uart = nx_factory_uart_ref(NX_UART_REF_RECEIVE);
    slots[0].data = blocks[0];
    slots[0].capacity = sizeof(blocks[0]);
    slots[1].data = blocks[1];
    slots[1].capacity = sizeof(blocks[1]);
    if (nx_stream_initialize(&stream, slots, 2U) != NX_SUCCESS ||
        nx_uart_port_rx_start(uart, &stream) != NX_SUCCESS)
        return 2;
    nx_uart_port_service(uart);
    if (nx_uart_port_rx_stop(uart) != NX_SUCCESS)
        return 3;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 4;
}
