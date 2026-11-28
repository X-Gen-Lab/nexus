/**
 * \file            main.c
 * \brief           Concurrent STM32F107VC remapped USART2 and Clause 22 MDIO.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_uart_port_t* const console =
        nx_factory_uart_ref(NX_UART_REF_CONSOLE);
    const nx_mdio_port_t* const phy = nx_factory_mdio_ref(NX_MDIO_REF_PHY);
    size_t count = 0U;
    uint8_t byte;
    uint16_t status;
    const nx_result_t serial =
        nx_uart_port_read_bytes(console, &byte, 1U, &count);
    const nx_result_t management = nx_mdio_read(
        phy, 1U, 1U, &status, nx_deadline_after(nx_time_now_us(), 1000U));
    const nx_result_t stopped = nx_platform_stop();
    return serial == NX_SUCCESS && management == NX_SUCCESS &&
                   stopped == NX_SUCCESS
               ? 0
               : 2;
}
