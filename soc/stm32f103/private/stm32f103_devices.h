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
#include "../../common/legacy_devices.h"
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
#ifdef __cplusplus
}
#endif
#endif
