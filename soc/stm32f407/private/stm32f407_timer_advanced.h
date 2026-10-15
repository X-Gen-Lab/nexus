/**
 * \file            stm32f407_timer_advanced.h
 * \brief           Private fixed timer modes and cold assembly leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F407_TIMER_ADVANCED_H
#define NX_STM32F407_TIMER_ADVANCED_H
#include "timer_advanced_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Configure the exclusive fixed timer, preserving Board/vector leases.
 */
nx_result_t
nx_stm32_timer_capture_initialize(nx_timer_capture_controller_t* port,
                                  const nx_timer_capture_config_t* config);
/** \brief End a drained session after assembly joins its IRQ publisher. */
nx_result_t nx_stm32_timer_capture_release(nx_timer_capture_controller_t* port);
/** \brief Bounded capture dispatch from this timer's static generated vector.
 */
void nx_stm32_timer_capture_irq(nx_timer_capture_controller_t* port);
/** \brief Cold configure exact deadtime, BKIN and inactive complementary
 * levels. */
nx_result_t
nx_stm32_advanced_pwm_initialize(nx_advanced_pwm_controller_t* port,
                                 const nx_advanced_pwm_config_t* config);
/** \brief End the stopped static session; Board/vector leases remain external.
 */
nx_result_t nx_stm32_advanced_pwm_release(nx_advanced_pwm_controller_t* port);
/** \brief Shared-vector owner dispatches only the observed hardware break. */
void nx_stm32_advanced_pwm_irq(nx_advanced_pwm_controller_t* port);
/** \brief Static shared methods; no registry, allocation or GPIO discovery. */
extern const nx_timer_capture_ops_t nx_stm32_timer_capture_ops;
/** \brief Static five-control table; independent of the default PWM mode. */
extern const nx_advanced_pwm_ops_t nx_stm32_advanced_pwm_ops;
#ifdef __cplusplus
}
#endif
#endif
