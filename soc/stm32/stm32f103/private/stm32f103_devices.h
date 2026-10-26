/**
 * \file            stm32f103_devices.h
 * \brief           Exact F103RB private physical Flash ADC and PWM bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F103_DEVICES_H
#define NX_STM32F103_DEVICES_H
#include "../../../../drivers/adc/legacy_adc/legacy_adc.h"
#include "../../../../drivers/flash/legacy_flash/legacy_flash.h"
#include "../../../../drivers/timer/legacy_pwm/legacy_pwm.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef nx_legacy_adc_state_t nx_stm32f103_adc_state_t;
typedef nx_legacy_pwm_state_t nx_stm32f103_pwm_state_t;
typedef nx_legacy_flash_state_t nx_stm32f103_flash_state_t;
#define nx_stm32f103_adc_ops   nx_legacy_adc_ops
#define nx_stm32f103_pwm_ops   nx_legacy_pwm_ops
#define nx_stm32f103_flash_ops nx_legacy_flash_ops
extern const nx_flash_geometry_t g_nx_stm32f103_flash_geometry;
/** \brief Bind the exact PA0 ADC clock and reviewed calibration delay.
 * \param[in,out] port: Cold runtime; retained/live owners remain unchanged.
 * \param[in] reference_mv: Validated Board reference within2700..3600mV.
 * \return Success/error; partial calibration requires legacy_adc_close.
 */
nx_result_t nx_stm32f103_adc_prepare(nx_stm32f103_adc_state_t* port,
                                     uint32_t reference_mv);
/** \brief Bind the exact8MHz PA6 PWM route and Board inactive level.
 * \param[in,out] port: Cold runtime; retained/live owners remain unchanged.
 * \param[in] tick_hz: Validated exact divisor of the fixed input clock.
 * \param[in] period_ticks: Validated fixed timer period.
 * \param[in] duty_ticks: Initial duty, mutable only through the PWM API.
 * \param[in] inactive: Board-declared PA6 inactive latch,0or64.
 * \return Success/error; partial setup requires legacy_pwm_close.
 */
nx_result_t nx_stm32f103_pwm_prepare(nx_stm32f103_pwm_state_t* port,
                                     uint32_t tick_hz, uint32_t period_ticks,
                                     uint32_t duty_ticks, uint32_t inactive);
/** \brief Bind exact physical geometry without erase/program side effects.
 * \param[in,out] port: Cold runtime; initialized/faulted owners cannot rebind.
 * \param[in] supply_mv: Board-declared supply within2700..3600mV.
 * \return Success or unchanged hardware rejection; no partition is inferred.
 */
nx_result_t nx_stm32f103_flash_prepare(nx_stm32f103_flash_state_t* port,
                                       uint32_t supply_mv);

#ifdef __cplusplus
}
#endif
#endif
