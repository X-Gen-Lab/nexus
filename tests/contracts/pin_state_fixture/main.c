/**
 * \file            main.c
 * \brief           Public selected-state, ordinary and empty ARM link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_G474_IO_FIXTURE
static nx_uart_tx_request_t s_request;
static uint8_t s_data[2] = {0x12U, 0x34U};
#endif
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS)
        return (int)result.primary;
#ifdef NX_PIN_STATE_FIXTURE
    const nx_pin_state_port_t* pins =
        nx_factory_pin_state(NX_PIN_STATE_ID_STATUS);
    nx_result_t selected = nx_pin_state_select(pins, NX_PIN_STATE_SLEEP);
    if (selected != NX_SUCCESS)
        return (int)selected;
    selected = nx_pin_state_select(pins, NX_PIN_STATE_DEFAULT);
    if (selected != NX_SUCCESS)
        return (int)selected;
#elif defined(NX_G474_IO_FIXTURE)
    /* Only the existing g474_assembly/io.toml comparison selects this branch.
     */
    const nx_gpio_port_t* led = nx_factory_gpio(NX_GPIO_ID_LED);
    const nx_uart_port_t* console = nx_factory_uart(NX_UART_ID_CONSOLE);
    nx_request_initialize(&s_request.base);
    (void)nx_gpio_port_write(led, 1U << 5U, 0U);
    (void)nx_uart_tx_prepare(&s_request, s_data, sizeof(s_data),
                             nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_uart_port_submit(console, &s_request);
    (void)nx_uart_port_cancel(console, &s_request);
    nx_uart_port_service(console);
#endif
    return (int)nx_platform_stop();
}
