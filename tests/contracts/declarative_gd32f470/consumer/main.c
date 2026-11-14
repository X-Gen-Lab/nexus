/**
 * \file            main.c
 * \brief           Public strong-factory consumption of a GD32 reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/gpio.h"
#include "nexus/io/uart.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Exercise static identity and lifecycle without product policy. */
int main(void) {
    const nx_gpio_port_t* gpio = nx_factory_gpio_ref(NX_GPIO_REF_STATUS);
    if (gpio == NULL)
        return 1;
#ifdef NX_UART_REF_CONSOLE
    nx_uart_tx_binding_t uart =
        nx_factory_uart_tx_binding_ref(NX_UART_REF_CONSOLE);
    if (uart.transport == NULL || uart.validate == NULL)
        return 1;
#endif
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return 2;
    uint32_t observed = 0U;
    nx_result_t result = nx_gpio_port_read(gpio, &observed);
    if (result != NX_SUCCESS)
        return 3;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 4;
}
