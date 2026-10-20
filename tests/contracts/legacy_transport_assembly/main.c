/**
 * \file            main.c
 * \brief           External typed factory transport link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_LEGACY_FREERTOS
#include "nexus/os/freertos.h"
static nx_freertos_task_t s_task;
_Alignas(8) static StackType_t s_stack[128];
static void entry(void* context) {
    (void)context;
}
#endif
#ifdef NX_LEGACY_UART
static nx_uart_tx_request_t s_tx;
static uint8_t s_data[4] = {1U, 2U, 3U, 4U};
#ifdef NX_LEGACY_BLOCKS
static uint8_t s_rx[2][16];
static nx_stream_slot_t s_slots[2];
static nx_stream_t s_stream;
#endif
#endif
#ifdef NX_LEGACY_I2C
static nx_i2c_request_t s_i2c;
static uint8_t s_i2c_data[2] = {0U, 1U};
static nx_i2c_message_t s_message = {s_i2c_data, sizeof(s_i2c_data), false};
#endif
int main(void) {
    nx_platform_diagnostic_t diagnostic;
    nx_platform_start_result_t start = nx_platform_start_diagnosed(&diagnostic);
    if (start.primary != NX_SUCCESS)
        return (int)start.primary;
#ifdef NX_LEGACY_UART
    const nx_uart_port_t* uart = nx_factory_uart(0U);
    nx_request_initialize(&s_tx.base);
    (void)nx_uart_tx_prepare(&s_tx, s_data, sizeof(s_data),
                             nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_uart_port_submit(uart, &s_tx);
    (void)nx_uart_port_cancel(uart, &s_tx);
    nx_uart_port_service(uart);
#ifdef NX_LEGACY_BLOCKS
    for (size_t i = 0U; i < 2U; ++i) {
        s_slots[i].data = s_rx[i];
        s_slots[i].capacity = sizeof(s_rx[i]);
    }
    (void)nx_stream_initialize(&s_stream, s_slots, 2U);
    (void)nx_uart_port_rx_start(uart, &s_stream);
    nx_uart_port_service(uart);
    (void)nx_uart_port_rx_stop(uart);
#endif
#endif
#ifdef NX_LEGACY_I2C
    const nx_i2c_port_t* i2c = nx_factory_i2c(0U);
    nx_request_initialize(&s_i2c.base);
    (void)nx_i2c_request_prepare(&s_i2c, &s_message, 1U,
                                 nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_i2c_endpoint_submit(nx_factory_i2c_device(0U), &s_i2c);
    (void)nx_i2c_port_cancel(i2c, &s_i2c);
    nx_i2c_port_service(i2c);
#endif
#ifdef NX_LEGACY_FREERTOS
    (void)nx_freertos_task_start(&s_task, "legacy-link", s_stack, 128U, 1U,
                                 entry, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
#else
    return (int)nx_platform_stop_diagnosed(&diagnostic);
#endif
}
