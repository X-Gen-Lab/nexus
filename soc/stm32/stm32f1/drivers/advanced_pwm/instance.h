/**
 * \file            instance.h
 * \brief           Private F103 CH1 complementary output owner.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_ADVANCED_PWM_INSTANCE_H
#define NEXUS_STM32F1_ADVANCED_PWM_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/advanced_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t period_ticks;
    uint32_t tick_hz;
    uint32_t deadtime_ticks;
    uint8_t priority;
    bool active_high;
    bool idle_high;
    bool idle_complement_high;
} nx_stm32f1_advanced_pwm_config_t;

typedef struct {
    const nx_stm32f1_advanced_pwm_config_t* config;
    nx_stm32f1_pads_t main_pads;
    nx_stm32f1_pads_t other_pads;
    uint16_t duty;
    uint8_t old_priority;
    uint8_t effects;
    volatile bool fault;
} nx_stm32f1_advanced_pwm_t;

/** \brief Prepare the selected timer outputs with GPIO idle and break armed. */
nx_result_t
nx_stm32f1_advanced_pwm_prepare(nx_stm32f1_advanced_pwm_t* state,
                                const nx_stm32f1_advanced_pwm_config_t* config);

/** \brief Prove output withdrawal and return the exact cold leases. */
nx_result_t nx_stm32f1_advanced_pwm_close(nx_stm32f1_advanced_pwm_t* state);

/** \brief Latch the selected timer break and mask its interrupt until explicit
 * recovery.
 */
void nx_stm32f1_advanced_pwm_irq(nx_stm32f1_advanced_pwm_t* state);

#if defined(NX_STM32F1_PWM_ADC_OWNER)
nx_result_t nx_stm32f1_advanced_pwm_one_pulse(nx_stm32f1_advanced_pwm_t* state,
                                              const uint32_t* duties,
                                              uint32_t phase);
#endif

extern const nx_advanced_pwm_ops_t nx_stm32f1_advanced_pwm_ops;

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F1_ADVANCED_PWM_INSTANCE_H */
