/**
 * \file            pwm_adc_cold.c
 * \brief           Exact finite PWM/ADC pads and COMP1 cold lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_control_cold.h"
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
nx_result_t
nx_stm32g474_pwm_adc_prepare(nx_stm32g474_pwm_adc_state_t* port,
                             const nx_stm32g474_pwm_adc_config_t* config,
                             nx_stm32g474_control_cold_t* receipt) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || receipt->comparator_retained)
        return NX_ERROR_BUSY;
    /* Reads follow Platform's observed acquisition of the control clocks. */
    (void)NX_G474_READ(&RCC->APB2ENR);
    (void)NX_G474_READ(&RCC->APB2ENR);
    if ((NX_G474_READ(&RCC->APB2RSTR) &
         (RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_SYSCFGRST)) != 0U ||
        NX_G474_READ(&TIM1->CR1) != 0U || NX_G474_READ(&TIM1->CCER) != 0U ||
        NX_G474_READ(&TIM1->DIER) != 0U || NX_G474_READ(&TIM1->BDTR) != 0U ||
        (NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != 0U ||
        !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn) ||
        !NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn) ||
        !NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn))
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32g474_control_inputs_prepare(
        receipt, 783U, COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1);
    if (result != NX_SUCCESS)
        return result;
    for (unsigned pin = 8U; pin <= 10U; ++pin) {
        result = nx_stm32g474_pin_alternate(GPIOA, pin, 6U);
        if (result != NX_SUCCESS)
            return result;
    }
    return nx_stm32g474_pwm_adc_initialize(port, config);
}
nx_result_t nx_stm32g474_pwm_adc_release(nx_stm32g474_pwm_adc_state_t* port,
                                         nx_stm32g474_control_cold_t* receipt) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32g474_pwm_adc_close(port);
    if (result != NX_SUCCESS)
        return result;
    return nx_stm32g474_control_inputs_release(receipt);
}
