/**
 * \file            h743-spi-dma.c
 * \brief           Public finite SPI DMA and optional UART link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_spi_request_t s_spi_request;
static nx_uart_tx_request_t s_uart_request;
_Alignas(32) static uint8_t s_tx[32] = {0x12U, 0x34U};
_Alignas(32) static uint8_t s_rx[32];
_Alignas(32) static uint8_t s_uart_rx[2][64];
static nx_stream_slot_t s_slots[2] = {
    {.data = s_uart_rx[0], .capacity = sizeof(s_uart_rx[0])},
    {.data = s_uart_rx[1], .capacity = sizeof(s_uart_rx[1])}};
static nx_stream_t s_stream;

/** \brief Preserve static borrowed storage when withdrawal cannot complete. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Stop outer leases, including partial-start cleanup responsibility. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}

/** \brief Link public calls; this firmware is not executed as a host test. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_spi_port_t* bus = nx_factory_spi(NX_SPI_ID_BUS4);
    const nx_spi_endpoint_t* endpoint =
        nx_factory_spi_device(NX_SPI_DEVICE_ID_DEVICE4);
    nx_request_initialize(&s_spi_request.base);
    if (bus == NULL || !nx_spi_endpoint_on_port(endpoint, bus) ||
        nx_spi_request_prepare(&s_spi_request, s_tx, s_rx, sizeof(s_tx),
                               nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_spi_endpoint_submit(endpoint, &s_spi_request) != NX_SUCCESS) {
        return close_platform(2);
    }
    (void)nx_spi_port_cancel(bus, &s_spi_request);
    for (unsigned i = 0U;
         i < 64U && nx_request_state(&s_spi_request.base) != NX_REQUEST_SETTLED;
         ++i) {
        nx_spi_port_service(bus);
    }
    (void)nx_spi_port_stop(bus);
    if (nx_request_state(&s_spi_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_spi_request.base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
    }
    for (nx_uart_id_t id = 0U; id != NX_UART_ID_COUNT; ++id) {
        const nx_uart_port_t* uart = nx_factory_uart(id);
        if (uart == NULL ||
            nx_stream_initialize(&s_stream, s_slots, 2U) != NX_SUCCESS) {
            return close_platform(3);
        }
        nx_result_t receive = nx_uart_port_rx_start(uart, &s_stream);
        if (receive != NX_SUCCESS && receive != NX_ERROR_UNSUPPORTED) {
            return close_platform(4);
        }
        nx_request_initialize(&s_uart_request.base);
        if (nx_uart_tx_prepare(&s_uart_request, s_tx, sizeof(s_tx),
                               nx_deadline_after(nx_time_now_us(), 10000U)) !=
                NX_SUCCESS ||
            nx_uart_port_submit(uart, &s_uart_request) != NX_SUCCESS) {
            return close_platform(5);
        }
        (void)nx_uart_port_cancel(uart, &s_uart_request);
        for (unsigned i = 0U;
             i < 64U &&
             nx_request_state(&s_uart_request.base) != NX_REQUEST_SETTLED;
             ++i) {
            nx_uart_port_service(uart);
        }
        (void)nx_uart_port_rx_stop(uart);
        (void)nx_uart_port_stop(uart);
        if (nx_request_state(&s_uart_request.base) != NX_REQUEST_SETTLED) {
            (void)nx_platform_stop();
            if (nx_request_state(&s_uart_request.base) != NX_REQUEST_SETTLED) {
                keep_storage();
            }
        }
    }
    return close_platform(0);
}
