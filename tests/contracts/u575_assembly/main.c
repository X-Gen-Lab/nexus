/**
 * \file            main.c
 * \brief           U575 link-only selected factory and actual kernel consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_U575_FREERTOS_FIXTURE
#include "nexus/os/freertos.h"
static nx_freertos_task_t s_task;
_Alignas(8) static StackType_t s_task_stack[128];
static void task_entry(void* context) {
    (void)context;
}
#endif
#ifdef NX_U575_IO_FIXTURE
static nx_uart_tx_request_t s_request;
static uint8_t s_data[2] = {0x12U, 0x34U};
#endif
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return (int)start.primary;
    }
#ifdef NX_U575_IO_FIXTURE
    const nx_gpio_port_t* led = nx_factory_gpio(NX_GPIO_ID_LED);
    const nx_uart_port_t* console = nx_factory_uart(NX_UART_ID_CONSOLE);
    nx_request_initialize(&s_request.base);
    (void)nx_gpio_port_write(led, 1U << 7U, 0U);
    (void)nx_uart_tx_prepare(&s_request, s_data, sizeof(s_data),
                             nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_uart_port_submit(console, &s_request);
    (void)nx_uart_port_cancel(console, &s_request);
    nx_uart_port_service(console);
#endif
#ifdef NX_U575_FREERTOS_FIXTURE
    /* Retain the real static kernel graph. This ELF is not executed by HIL. */
    (void)nx_freertos_task_start(&s_task, "u5-link", s_task_stack, 128U, 1U,
                                 task_entry, NULL);
    vTaskStartScheduler();
    /* A returning scheduler may have run tasks. Never roll it back here. */
    for (;;) {
    }
#else
    return (int)nx_platform_stop();
#endif
}
