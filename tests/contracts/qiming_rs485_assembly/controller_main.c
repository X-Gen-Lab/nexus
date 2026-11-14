/**
 * \file            controller_main.c
 * \brief           Public strong-factory RS485 composition and retained drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         ARM linkage only; no transceiver or physical execution
 * proof.
 */
#include "nexus/io/uart_rs485.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "nexus_factory_strong.h"

static nx_uart_rs485_state_t s_owner;
static nx_uart_rs485_config_t s_config;
static const nx_uart_port_t s_bus = {&nx_uart_rs485_ops, &s_owner};
static nx_uart_tx_request_t s_request;
static const uint8_t s_payload[] = {0x55U, 0xaaU};

/** \brief Keep every caller-owned descriptor alive on uncertain withdrawal. */
static void retain(void) {
    for (;;) {
    }
}

/** \brief Bind callback and port from one nominal identity, with no SDK types.
 */
int main(void) {
    const nx_uart_ref_t identity = NX_UART_REF_SERIAL2;
    const nx_uart_port_t* transport = nx_factory_uart_ref(identity);
    nx_uart_tx_validate_fn validate =
        nx_factory_uart_tx_validator_ref(identity);
    const nx_gpio_signal_t* direction =
        nx_factory_gpio_signal_ref(NX_GPIO_REF_DIRECTION);
    const volatile nx_uart_port_t* observed_uart = transport;
    const volatile nx_gpio_signal_t* observed_signal = direction;
    if (validate == NULL || observed_uart == NULL ||
        observed_uart->ops == NULL || observed_uart->context == NULL ||
        observed_signal == NULL || observed_signal->port == NULL ||
        observed_signal->port->ops == NULL ||
        observed_signal->port->context == NULL)
        return 1;
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        retain();
    /* The fixture owns these explicit timing assumptions. Product timing
     * requires the actual powered transceiver and verified bus conditions. */
    s_config = (nx_uart_rs485_config_t){.transport = transport,
                                        .direction = direction,
                                        .validate = validate,
                                        .setup_us = 10U,
                                        .hold_us = 20U};
    if (nx_uart_rs485_initialize(&s_owner, &s_config) != NX_SUCCESS)
        retain();
    nx_time_us_t deadline;
    nx_request_initialize(&s_request.base);
    if (nx_deadline_finite_after(nx_time_now_us(), 100000U, &deadline) !=
            NX_SUCCESS ||
        nx_uart_tx_prepare(&s_request, s_payload, sizeof(s_payload),
                           deadline) != NX_SUCCESS ||
        nx_uart_port_submit(&s_bus, &s_request) != NX_SUCCESS)
        retain();
    bool cancelled = false;
    while (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        nx_uart_port_service(&s_bus);
        if (!cancelled && nx_deadline_expired(deadline, nx_time_now_us())) {
            (void)nx_uart_port_cancel(&s_bus, &s_request);
            cancelled = true;
        }
        /* Deadline/cancel does not revoke storage. TC failure can leave this
         * loop owning QUARANTINED bytes indefinitely; no fake release follows.
         */
    }
    if (nx_uart_port_stop(&s_bus) != NX_SUCCESS)
        retain();
    (void)nx_platform_stop();
    retain();
    return 0;
}
