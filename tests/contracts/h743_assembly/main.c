/**
 * \file            main.c
 * \brief           H743 link-only selected factory and actual kernel consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_H743_FREERTOS_FIXTURE
#include "nexus/os/freertos.h"
static nx_freertos_task_t s_task;
_Alignas(8) static StackType_t s_task_stack[128];
static void task_entry(void* context) {
    (void)context;
}
#endif
#ifdef NX_H743_IO_FIXTURE
static nx_uart_tx_request_t s_request;
#ifdef NX_H743_DMA_FIXTURE
_Alignas(32) static uint8_t s_data[32] = {0x12U, 0x34U};
#else
static uint8_t s_data[2] = {0x12U, 0x34U};
#endif
#endif
#ifdef NX_H743_RX_FIXTURE
_Alignas(32) static uint8_t s_rx_data[2][64];
static nx_stream_slot_t s_rx_slots[2] = {
    {.data = s_rx_data[0], .capacity = sizeof(s_rx_data[0])},
    {.data = s_rx_data[1], .capacity = sizeof(s_rx_data[1])}};
static nx_stream_t s_rx_stream;
#endif
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return (int)start.primary;
    }
#ifdef NX_H743_IO_FIXTURE
    const nx_gpio_port_t* led = nx_factory_gpio(NX_GPIO_ID_LED);
    const nx_uart_port_t* console = nx_factory_uart(NX_UART_ID_CONSOLE);
#ifdef NX_H743_RX_FIXTURE
    (void)nx_stream_initialize(&s_rx_stream, s_rx_slots, 2U);
    (void)nx_uart_port_rx_start(console, &s_rx_stream);
#endif
    nx_request_initialize(&s_request.base);
    (void)nx_gpio_port_write(led, 1U << 0U, 0U);
    (void)nx_uart_tx_prepare(&s_request, s_data, sizeof(s_data),
                             nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_uart_port_submit(console, &s_request);
    (void)nx_uart_port_cancel(console, &s_request);
    nx_uart_port_service(console);
#ifdef NX_H743_RX_FIXTURE
    (void)nx_uart_port_rx_stop(console);
#endif
#endif
#ifdef NX_H743_FREERTOS_FIXTURE
    /* Retain the real static kernel graph. This ELF is not executed by HIL. */
    (void)nx_freertos_task_start(&s_task, "h7-link", s_task_stack, 128U, 1U,
                                 task_entry, NULL);
    vTaskStartScheduler();
    /* A returning scheduler may have run tasks. Never roll it back here. */
    for (;;) {
    }
#else
    return (int)nx_platform_stop();
#endif
}
