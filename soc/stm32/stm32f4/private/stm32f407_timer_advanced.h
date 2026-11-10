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
/** \brief The second advanced controller keeps the same state/config ABI. */
nx_result_t
nx_stm32_tim8_advanced_pwm_initialize(nx_advanced_pwm_controller_t* port,
                                      const nx_advanced_pwm_config_t* config);
/** \brief Release only the drained TIM8 session. */
nx_result_t
nx_stm32_tim8_advanced_pwm_release(nx_advanced_pwm_controller_t* port);
/** \brief Dispatch only TIM8's observed break from its static vector. */
void nx_stm32_tim8_advanced_pwm_irq(nx_advanced_pwm_controller_t* port);
extern const nx_advanced_pwm_ops_t nx_stm32_tim8_advanced_pwm_ops;
/** \brief Independent general timers retain their native counter width. */
nx_result_t
nx_stm32_tim3_timer_capture_initialize(nx_timer_capture_controller_t* port,
                                       const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim5_timer_capture_initialize(nx_timer_capture_controller_t* port,
                                       const nx_timer_capture_config_t* config);
/** \brief Release only the corresponding drained and quiescent session. */
nx_result_t
nx_stm32_tim3_timer_capture_release(nx_timer_capture_controller_t* port);
nx_result_t
nx_stm32_tim5_timer_capture_release(nx_timer_capture_controller_t* port);
/** \brief Static generated vectors deliver only their own controller. */
void nx_stm32_tim3_timer_capture_irq(nx_timer_capture_controller_t* port);
void nx_stm32_tim5_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim3_timer_capture_ops;
extern const nx_timer_capture_ops_t nx_stm32_tim5_timer_capture_ops;
/** \brief Dedicated CC vectors serve the advanced timers' input personality. */
nx_result_t
nx_stm32_tim1_timer_capture_initialize(nx_timer_capture_controller_t* port,
                                       const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim8_timer_capture_initialize(nx_timer_capture_controller_t* port,
                                       const nx_timer_capture_config_t* config);
/** \brief End the exact input session after CC-vector and ring withdrawal. */
nx_result_t
nx_stm32_tim1_timer_capture_release(nx_timer_capture_controller_t* port);
nx_result_t
nx_stm32_tim8_timer_capture_release(nx_timer_capture_controller_t* port);
/** \brief No shared break/update/trigger vector is consumed by capture. */
void nx_stm32_tim1_timer_capture_irq(nx_timer_capture_controller_t* port);
void nx_stm32_tim8_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim1_timer_capture_ops;
extern const nx_timer_capture_ops_t nx_stm32_tim8_timer_capture_ops;
/** \brief Small capture owners retain only their implemented CC registers. */
nx_result_t
nx_stm32_tim9_timer_capture_initialize(nx_timer_capture_controller_t* port,
                                       const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim9_timer_capture_release(nx_timer_capture_controller_t* port);
void nx_stm32_tim9_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim9_timer_capture_ops;
nx_result_t nx_stm32_tim10_timer_capture_initialize(
    nx_timer_capture_controller_t* port,
    const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim10_timer_capture_release(nx_timer_capture_controller_t* port);
void nx_stm32_tim10_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim10_timer_capture_ops;
nx_result_t nx_stm32_tim11_timer_capture_initialize(
    nx_timer_capture_controller_t* port,
    const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim11_timer_capture_release(nx_timer_capture_controller_t* port);
void nx_stm32_tim11_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim11_timer_capture_ops;
nx_result_t nx_stm32_tim12_timer_capture_initialize(
    nx_timer_capture_controller_t* port,
    const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim12_timer_capture_release(nx_timer_capture_controller_t* port);
void nx_stm32_tim12_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim12_timer_capture_ops;
nx_result_t nx_stm32_tim13_timer_capture_initialize(
    nx_timer_capture_controller_t* port,
    const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim13_timer_capture_release(nx_timer_capture_controller_t* port);
void nx_stm32_tim13_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim13_timer_capture_ops;
nx_result_t nx_stm32_tim14_timer_capture_initialize(
    nx_timer_capture_controller_t* port,
    const nx_timer_capture_config_t* config);
nx_result_t
nx_stm32_tim14_timer_capture_release(nx_timer_capture_controller_t* port);
void nx_stm32_tim14_timer_capture_irq(nx_timer_capture_controller_t* port);
extern const nx_timer_capture_ops_t nx_stm32_tim14_timer_capture_ops;

#ifdef __cplusplus
}
#endif
#endif
