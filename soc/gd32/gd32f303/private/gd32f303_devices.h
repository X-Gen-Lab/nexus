/**
 * \file            gd32f303_devices.h
 * \brief           Exact GD303RC private physical Flash ADC and PWM bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_GD32F303_DEVICES_H
#define NX_GD32F303_DEVICES_H
#include "../../../../drivers/adc/legacy_adc/legacy_adc.h"
#include "../../../../drivers/flash/legacy_flash/legacy_flash.h"
#include "../../../../drivers/timer/legacy_pwm/legacy_pwm.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef nx_legacy_adc_state_t nx_gd32f303_adc_state_t;
typedef nx_legacy_pwm_state_t nx_gd32f303_pwm_state_t;
typedef nx_legacy_flash_state_t nx_gd32f303_flash_state_t;
#define nx_gd32f303_adc_ops   nx_legacy_adc_ops
#define nx_gd32f303_pwm_ops   nx_legacy_pwm_ops
#define nx_gd32f303_flash_ops nx_legacy_flash_ops
extern const nx_flash_geometry_t g_nx_gd32f303_flash_geometry;
#ifdef __cplusplus
}
#endif
#endif
