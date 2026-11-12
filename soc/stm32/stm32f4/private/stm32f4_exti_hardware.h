/**
 * \file            stm32f4_exti_hardware.h
 * \brief           Queue-free EXTI event pulses with exclusive static ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_EXTI_HARDWARE_H
#define NEXUS_STM32F4_EXTI_HARDWARE_H
#include "stm32f407_provider.h"

/** \brief Immutable pad, mux, edge and pull selection; no IRQ or queue. */
typedef struct {
    GPIO_TypeDef* gpio;
    nx_exti_edge_t edge;
    uint8_t gpio_index;
    uint8_t line;
    uint8_t pull;
} nx_stm32_exti_hardware_config_t;

/** \brief Retained selected mux responsibility; shared Platform owns the pad.
 */
typedef struct {
    const nx_stm32_exti_hardware_config_t* config;
    uint8_t saved_mux;
    bool owned;
    bool initialized;
} nx_stm32_exti_hardware_state_t;

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_exti_ops_t nx_stm32_exti_hardware_ops;

/**
 * \brief Validate a selected hardware-event line without any register writes.
 * \param[in] state: Static zero or fully withdrawn exclusive instance.
 * \param[in] config: Static exact GPIO route and edge; retained only by
 * prepare. \return Success or INVALID, CONTEXT or BUSY, with no acquired
 * references. \note Privileged unmasked Thread owner; no IRQ/queue/notification
 * resources.
 */
nx_result_t
nx_stm32_exti_hardware_admit(const nx_stm32_exti_hardware_state_t* state,
                             const nx_stm32_exti_hardware_config_t* config);

/**
 * \brief Arm an EMR hardware pulse after configuring its exact input and mux.
 * \param[in,out] state: Static serialized owner, retained through failed close.
 * \param[in] config: Immutable facts; outlive the owner until close succeeds.
 * \return Success or error; owned retains every partial hardware effect.
 * \note Platform already owns the pad snapshot and GPIO/SYSCFG gates. No IRQ,
 *       software queue, callback or worker is installed. CPU event-latch and
 *       WFE policy belong to the caller; events may coalesce without a count.
 */
nx_result_t
nx_stm32_exti_hardware_prepare(nx_stm32_exti_hardware_state_t* state,
                               const nx_stm32_exti_hardware_config_t* config);

/**
 * \brief Withdraw only this EMR source/triggers and restore its mux nibble.
 * \param[in,out] state: Same privileged unmasked Thread owner as prepare.
 * \return Success proves source withdrawal; IO retains state/config for retry.
 * \note Shared IRQs and peer event sources remain untouched. Platform restores
 *       the pad after later owners close. An already-latched CPU event remains
 *       the caller's wait-protocol responsibility, not an event history queue.
 */
nx_result_t nx_stm32_exti_hardware_close(nx_stm32_exti_hardware_state_t* state);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F4_EXTI_HARDWARE_H */
