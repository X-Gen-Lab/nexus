/**
 * \file            stm32f407_uart_wire.h
 * \brief           Private USART wire-mode admission and initialization
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_UART_WIRE_H
#define NX_STM32F407_UART_WIRE_H

#include "nexus/io/uart_wire.h"
#include "stm32f407_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Validate exact instance/mode limits without writes or retention.
 * \param[in] port: Static UART instance, clock and receive profile facts.
 * \param[in] config: Immutable wire tuple; no pointer is retained.
 * \return Success, INVALID or UNSUPPORTED; no hardware effects on refusal.
 */
nx_result_t nx_stm32_uart_wire_validate(const nx_stm32_uart_state_t* port,
                                        const nx_uart_wire_config_t* config);
/** \brief Initialize a complete selected wire tuple before UE/IRQ enable.
 * \param[in,out] port: Idle caller-owned static state and ring storage.
 * \param[in] clock_hz: Selected exact APB clock in Hz.
 * \param[in] config: Validated immutable wire tuple; not retained.
 * \return Success or error; a failed withdrawal retains initialized state.
 * \note Privileged Thread startup; cold owner owns pads, gate and publisher.
 *       Every mode reuses byte/event TX/RX, cancellation, stop and TC drain.
 */
nx_result_t nx_stm32_uart_wire_initialize(nx_stm32_uart_state_t* port,
                                          uint32_t clock_hz,
                                          const nx_uart_wire_config_t* config);

/** \brief Internal complete pre-enable tuple used by reviewed wire owners.
 * \param[in,out] port: Uninitialized owner with static receive storage.
 * \param[in] clock_hz: Exact selected peripheral clock.
 * \param[in] frame: CR1 payload/parity selection, excluding UE and sources.
 * \param[in] control2: Complete CR2 framing and clock tuple.
 * \param[in] control3: Complete CR3 personality and flow tuple.
 * \param[in] guard_prescaler: Complete GTPR guard/prescaler tuple.
 * \return Success or retained initialization failure; Thread startup only.
 */
nx_result_t nx_stm32_uart_initialize_controls(nx_stm32_uart_state_t* port,
                                              uint32_t clock_hz, uint32_t frame,
                                              uint32_t control2,
                                              uint32_t control3,
                                              uint32_t guard_prescaler);

#ifdef __cplusplus
}
#endif
#endif
