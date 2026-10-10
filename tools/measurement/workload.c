/**
 * \file            workload.c
 * \brief           Minimal explicit platform resource linkage workload
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
#if defined(NEXUS_BACKEND_FREERTOS)
#include "nexus/os/freertos.h"
#endif

/** \brief           Exercise only the explicitly selected resource fixture. */
int main(void) {
    nx_platform_start_result_t startup = nx_platform_start();
    if (startup.primary != NX_SUCCESS) {
        return 1;
    }
#if defined(NEXUS_WORKLOAD_gpio)
    if (nx_gpio_port_toggle(nx_binding_led0, NEXUS_LED0_MASK) != NX_SUCCESS) {
        return 2;
    }
#elif defined(NEXUS_WORKLOAD_uart)
    static const uint8_t payload[] = {'N', 'X'};
    static nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 100000u);
    if (nx_uart_tx_prepare(&request, payload, sizeof(payload), deadline) !=
        NX_SUCCESS) {
        return 4;
    }
#if defined(NEXUS_UART1_SELECTED)
    const nx_uart_port_t* uart = nx_binding_uart1;
#else
    const nx_uart_port_t* uart = nx_binding_uart0;
#endif
    if (nx_uart_port_submit(uart, &request) != NX_SUCCESS) {
        return 5;
    }
    while (nx_request_state(&request.base) != NX_REQUEST_SETTLED) {
        nx_uart_port_service(uart);
    }
#elif defined(NEXUS_WORKLOAD_spi)
    uint8_t tx = 0x9fu;
    uint8_t rx = 0u;
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 100000u);
    size_t transferred = 0u;
    if (nx_spi_endpoint_transfer(nx_device_nor0, &tx, &rx, 1u, deadline,
                                 &transferred) != NX_SUCCESS) {
        return 7;
    }
#endif
#if defined(NEXUS_BACKEND_NATIVE)
    return nx_platform_stop() == NX_SUCCESS ? 0 : 8;
#elif defined(NEXUS_BACKEND_FREERTOS)
    vTaskStartScheduler();
    return 9;
#else
    for (;;) {
    }
#endif
}
