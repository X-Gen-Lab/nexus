/**
 * \file            pwm_group_cold.c
 * \brief           Exact TIM1 group pads, COMP1 settling and IRQ cold lease
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
nx_stm32g474_pwm_group_prepare(nx_stm32g474_pwm_group_state_t* port,
                               const nx_stm32g474_pwm_group_config_t* config,
                               nx_stm32g474_control_cold_t* receipt) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || receipt->comparator_retained ||
        receipt->vector_retained)
        return NX_ERROR_BUSY;
    (void)NX_G474_READ(&RCC->APB2ENR);
    (void)NX_G474_READ(&RCC->APB2ENR);
    if ((NX_G474_READ(&RCC->APB2RSTR) &
         (RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_SYSCFGRST)) != 0U ||
        (NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != 0U ||
        !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn))
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32g474_control_inputs_prepare(
        receipt, (3U << 2U) | (3U << 8U), config->comparator_csr);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32g474_pwm_group_initialize(port, config);
    if (result != NX_SUCCESS)
        return result;
    for (unsigned pin = 8U; pin <= 10U; ++pin) {
        result = nx_stm32g474_pin_alternate(GPIOA, pin, 6U);
        if (result != NX_SUCCESS)
            return result;
    }
    receipt->priority = (uint8_t)NX_G474_IRQ_PRIORITY(TIM1_BRK_TIM15_IRQn);
    receipt->vector_retained = true;
    return NX_G474_IRQ_CONTROL(TIM1_BRK_TIM15_IRQn, true, 5U);
}
nx_result_t
nx_stm32g474_pwm_group_release(nx_stm32g474_pwm_group_state_t* port,
                               nx_stm32g474_control_cold_t* receipt) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result;
    if (port->initialized) {
        const nx_pwm_group_t face = {&nx_stm32g474_pwm_group_ops, port};
        result = nx_pwm_group_stop(&face);
        if (result != NX_SUCCESS)
            return result;
    }
    if (receipt->vector_retained) {
        result = NX_G474_IRQ_CONTROL(TIM1_BRK_TIM15_IRQn, false, 0U);
        if (result != NX_SUCCESS)
            return result;
        if (!NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn))
            return NX_ERROR_BUSY;
        NX_G474_IRQ_PRIORITY_WRITE(TIM1_BRK_TIM15_IRQn, receipt->priority);
        nx_arch_dsb();
        nx_arch_isb();
        if (NX_G474_IRQ_PRIORITY(TIM1_BRK_TIM15_IRQn) != receipt->priority)
            return NX_ERROR_IO;
        receipt->vector_retained = false;
    }
    result = nx_stm32g474_pwm_group_close(port);
    if (result != NX_SUCCESS)
        return result;
    return nx_stm32g474_control_inputs_release(receipt);
}
