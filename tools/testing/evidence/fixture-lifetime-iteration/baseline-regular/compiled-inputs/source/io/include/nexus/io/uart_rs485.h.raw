/**
 * \file            uart_rs485.h
 * \brief           Optional single-owner RS485 direction and wire-tail adapter
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_RS485_H
#define NEXUS_UART_RS485_H

#include "nexus/io/gpio.h"
#include "nexus/io/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Exact transport's side-effect-free TX admission validator.
 * \param[in]       context: The immutable transport face's exact context.
 * \param[in]       request: Live READY or QUEUED descriptor, never retained.
 * \return          Same rejectable checks used by the transport submit/start.
 * \note            Reads clock/hardware facts but never writes GPIO/MMIO,
 *                  admits, starts or retains a reference. One executor owns
 *                  adapter and transport; no external direct TX is permitted.
 */
typedef nx_result_t (*nx_uart_tx_validate_fn)(
    void* context, const nx_uart_tx_request_t* request);

/**
 * \brief           Immutable selected electrical/timing facts, no task or heap.
 * \note            Configuration, faces, ops and context identities cannot
 *                  change while bound. They are disjoint from mutable adapter,
 *                  request and payload storage; exact pin ownership is
 * external.
 */
typedef struct {
    const nx_uart_port_t* transport;
    const nx_gpio_port_t* direction;
    nx_uart_tx_validate_fn validate;
    uint32_t mask;
    nx_time_us_t setup_us;
    nx_time_us_t hold_us;
    bool active_high;
} nx_uart_rs485_config_t;

/**
 * \brief           Caller-owned opt-in adapter, zero-initialize before init.
 * \note            Control is private after initialization. Immutable config,
 *                  both faces, pin ownership and all storage outlive successful
 *                  stop plus external observers. Default UART state is
 * unchanged.
 */
typedef struct {
    const nx_uart_rs485_config_t* config;
    nx_uart_tx_request_t inner;
    nx_uart_tx_request_t* active;
    nx_time_us_t boundary;
    nx_result_t terminal;
    size_t transferred;
    uint8_t phase;
    bool initialized;
    bool closing;
} nx_uart_rs485_state_t;

extern const nx_uart_ops_t nx_uart_rs485_ops;

/**
 * \brief           Bind exclusive transport and establish inactive DE.
 * \param[in,out]   state: Fresh zero-initialized caller control.
 * \param[in]       config: Immutable lifetime binding; one authorized DE bit,
 *                  real board polarity, setup/hold in nx_time_now_us domain.
 * \return          Success or INVALID/CONTEXT/STATE/GPIO failure, no TX loan.
 * \note            Task-only, no wait. No physical transceiver is inferred.
 *                  Setup starts after successful DE assertion; hold starts
 * after acquire-observed true transport settlement. Transfer deadline includes
 * setup and wire completion; mandatory drain/hold may extend the borrow beyond
 * that deadline. Rejected TX leaves DE and transport unchanged. Failed drain or
 * DE withdrawal keeps QUARANTINED outer storage and DE ownership until service
 * retry.
 */
nx_result_t nx_uart_rs485_initialize(nx_uart_rs485_state_t* state,
                                     const nx_uart_rs485_config_t* config);

#ifdef __cplusplus
}
#endif
#endif
