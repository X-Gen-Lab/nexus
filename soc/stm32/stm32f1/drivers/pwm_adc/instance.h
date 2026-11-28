/**
 * \file            instance.h
 * \brief           Private F103 finite TIM1/ADC1 cycle owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_PWM_ADC_INSTANCE_H
#define NEXUS_STM32F1_PWM_ADC_INSTANCE_H

#include "drivers/advanced_pwm/instance.h"
#include "nexus/io/pwm_adc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_advanced_pwm_config_t pwm;
    uint16_t phase;
    uint8_t sample;
} nx_stm32f1_cycle_config_t;

typedef struct {
    const nx_stm32f1_cycle_config_t* config;
    nx_stm32f1_advanced_pwm_t pwm;
    nx_stm32f1_pads_t analog;
    nx_pwm_adc_request_t* request;
    nx_result_t terminal;
    uint8_t update_priority;
    uint8_t effects;
    volatile uint8_t completed;
    bool fault;
    bool stopped;
} nx_stm32f1_cycle_t;

nx_result_t nx_stm32f1_cycle_prepare(nx_stm32f1_cycle_t* state,
                                     const nx_stm32f1_cycle_config_t* config);
nx_result_t nx_stm32f1_cycle_close(nx_stm32f1_cycle_t* state);
void nx_stm32f1_cycle_break_irq(nx_stm32f1_cycle_t* state);
void nx_stm32f1_cycle_update_irq(nx_stm32f1_cycle_t* state);
extern const nx_pwm_adc_ops_t nx_stm32f1_cycle_ops;

#ifdef __cplusplus
}
#endif
#endif
