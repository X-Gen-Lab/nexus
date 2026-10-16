/**
 * \file            main.c
 * \brief           External F103 factory consumer; link evidence only
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_F103_FREERTOS_FIXTURE
#include "nexus/os/freertos.h"
static nx_freertos_task_t s_task;
_Alignas(8) static StackType_t s_task_stack[128];
static void entry(void* context) {
    (void)context;
}
#endif
#ifdef NX_F103_IO_FIXTURE
static nx_uart_tx_request_t s_request;
static const uint8_t s_data[] = {0x12U, 0x34U};
#endif
int main(void) {
    nx_platform_diagnostic_t diagnostic;
    nx_platform_start_result_t start = nx_platform_start_diagnosed(&diagnostic);
    if (start.primary != NX_SUCCESS) {
        return (int)start.primary;
    }
#ifdef NX_F103_IO_FIXTURE
    const nx_gpio_port_t* led = nx_factory_gpio(NX_GPIO_ID_LED0);
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_UART1);
    nx_request_initialize(&s_request.base);
    (void)nx_gpio_port_write(led, 32U, 0U);
    (void)nx_uart_tx_prepare(&s_request, s_data, sizeof(s_data),
                             nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_uart_port_submit(uart, &s_request);
    (void)nx_uart_port_cancel(uart, &s_request);
    nx_uart_port_service(uart);
    uint8_t received[2];
    size_t count = 0U;
    (void)nx_uart_port_read_bytes(uart, received, sizeof(received), &count);
    (void)nx_uart_port_attach_wake(uart, NULL, 0U);
#endif
#ifdef NX_F103_SPI_FIXTURE
    const nx_spi_port_t* spi = nx_factory_spi(NX_SPI_ID_BUS);
    const nx_spi_endpoint_t* endpoint =
        nx_factory_spi_device(NX_SPI_DEVICE_ID_PERIPHERAL);
    uint8_t tx[2] = {0x12U, 0x34U};
    uint8_t rx[2];
    size_t transferred = 0U;
    (void)nx_spi_endpoint_transfer(endpoint, tx, rx, sizeof(tx),
                                   nx_deadline_after(nx_time_now_us(), 1000U),
                                   &transferred);
    (void)nx_spi_port_recover(spi);
#endif
#ifdef NX_F103_EXTI_FIXTURE
    const nx_exti_port_t* edge0 = nx_factory_exti(NX_EXTI_ID_EDGE0);
    const nx_exti_port_t* edge1 = nx_factory_exti(NX_EXTI_ID_EDGE1);
    nx_exti_event_t events[2];
    (void)nx_exti_port_read(edge0, events, 2U, &count);
    (void)nx_exti_port_read(edge1, events, 2U, &count);
    (void)nx_exti_port_attach_wake(edge0, NULL, 0U);
#endif
#ifdef NX_F103_WATCHDOG_FIXTURE
    const nx_watchdog_port_t* watchdog =
        nx_factory_watchdog(NX_WATCHDOG_ID_IWDG);
    nx_watchdog_state_t state;
    (void)nx_watchdog_port_state(watchdog, &state);
#endif
#ifdef NX_F103_FREERTOS_FIXTURE
    (void)nx_freertos_task_start(&s_task, "f1-link", s_task_stack, 128U, 1U,
                                 entry, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
#else
    return (int)nx_platform_stop_diagnosed(&diagnostic);
#endif
}
