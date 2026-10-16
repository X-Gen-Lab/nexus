/**
 * \file            main.c
 * \brief           Public static factory consumer for software link evidence
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_MAINSTREAM_FREERTOS
#include "nexus/os/freertos.h"
static nx_freertos_task_t s_task;
_Alignas(8) static StackType_t s_task_stack[128];
static void entry(void* context) {
    (void)context;
}
#endif
#ifdef NX_MAINSTREAM_UART
static nx_uart_tx_request_t s_requests[NX_UART_ID_COUNT];
static const uint8_t s_uart_data[] = {0x12U, 0x34U};
#endif
#ifdef NX_MAINSTREAM_SPI
static const uint8_t s_spi_tx[] = {0x12U, 0x34U};
static uint8_t s_spi_rx[NX_SPI_DEVICE_ID_COUNT][sizeof(s_spi_tx)];
#endif
int main(void) {
    nx_platform_diagnostic_t diagnostic;
    nx_platform_start_result_t start = nx_platform_start_diagnosed(&diagnostic);
    if (start.primary != NX_SUCCESS)
        return (int)start.primary;
#ifdef NX_MAINSTREAM_GPIO
    for (nx_gpio_id_t id = 0U; id < NX_GPIO_ID_COUNT; ++id) {
        uint32_t value;
        (void)nx_gpio_port_read(nx_factory_gpio(id), &value);
    }
#endif
#ifdef NX_MAINSTREAM_UART
    for (nx_uart_id_t id = 0U; id < NX_UART_ID_COUNT; ++id) {
        const nx_uart_port_t* uart = nx_factory_uart(id);
        nx_request_initialize(&s_requests[id].base);
        (void)nx_uart_tx_prepare(&s_requests[id], s_uart_data,
                                 sizeof(s_uart_data),
                                 nx_deadline_after(nx_time_now_us(), 1000U));
        (void)nx_uart_port_submit(uart, &s_requests[id]);
        (void)nx_uart_port_cancel(uart, &s_requests[id]);
        nx_uart_port_service(uart);
        uint8_t received[2];
        size_t count;
        (void)nx_uart_port_read_bytes(uart, received, sizeof(received), &count);
    }
#endif
#ifdef NX_MAINSTREAM_SPI
    for (nx_spi_device_id_t id = 0U; id < NX_SPI_DEVICE_ID_COUNT; ++id) {
        size_t transferred;
        (void)nx_spi_endpoint_transfer(
            nx_factory_spi_device(id), s_spi_tx, s_spi_rx[id], sizeof(s_spi_tx),
            nx_deadline_after(nx_time_now_us(), 1000U), &transferred);
    }
    for (nx_spi_id_t id = 0U; id < NX_SPI_ID_COUNT; ++id)
        (void)nx_spi_port_recover(nx_factory_spi(id));
#endif
#ifdef NX_MAINSTREAM_EXTI
    for (nx_exti_id_t id = 0U; id < NX_EXTI_ID_COUNT; ++id) {
        nx_exti_event_t events[2];
        size_t count;
        (void)nx_exti_port_read(nx_factory_exti(id), events, 2U, &count);
    }
#endif
#ifdef NX_MAINSTREAM_WATCHDOG
    for (nx_watchdog_id_t id = 0U; id < NX_WATCHDOG_ID_COUNT; ++id) {
        nx_watchdog_state_t state;
        (void)nx_watchdog_port_state(nx_factory_watchdog(id), &state);
    }
#endif
#ifdef NX_MAINSTREAM_FREERTOS
    /* Retain the real static kernel graph; this image is never a HIL result. */
    (void)nx_freertos_task_start(&s_task, "mainstream-link", s_task_stack, 128U,
                                 1U, entry, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
#else
    return (int)nx_platform_stop_diagnosed(&diagnostic);
#endif
}
