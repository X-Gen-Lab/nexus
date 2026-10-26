/**
 * \file            devices.c
 * \brief           Exact legacy device register review and physical geometry
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f103_provider.h"
#include <stddef.h>
_Static_assert(offsetof(nx_legacy_adc_registers_t, control2) ==
                   offsetof(ADC_TypeDef, CR2),
               "Exact F103 CR2 register ABI");
_Static_assert(offsetof(nx_legacy_adc_registers_t, sample2) ==
                   offsetof(ADC_TypeDef, SMPR2),
               "Exact F103 SMPR2 register ABI");
_Static_assert(offsetof(nx_legacy_adc_registers_t, sequence3) ==
                   offsetof(ADC_TypeDef, SQR3),
               "Exact F103 SQR3 register ABI");
_Static_assert(offsetof(nx_legacy_adc_registers_t, data) ==
                   offsetof(ADC_TypeDef, DR),
               "Exact F103 DR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, key) ==
                   offsetof(FLASH_TypeDef, KEYR),
               "Exact F103 KEYR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, status) ==
                   offsetof(FLASH_TypeDef, SR),
               "Exact F103 SR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, control) ==
                   offsetof(FLASH_TypeDef, CR),
               "Exact F103 CR register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, address) ==
                   offsetof(FLASH_TypeDef, AR),
               "Exact F103 AR register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, channel_mode1) ==
                   offsetof(TIM_TypeDef, CCMR1),
               "Exact F103 CCMR1 register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, prescaler) ==
                   offsetof(TIM_TypeDef, PSC),
               "Exact F103 PSC register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, reload) ==
                   offsetof(TIM_TypeDef, ARR),
               "Exact F103 ARR register ABI");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, compare1) ==
                   offsetof(TIM_TypeDef, CCR1),
               "Exact F103 CCR1 register ABI");
_Static_assert(ADC_CR2_ADON == 1U && ADC_CR2_CAL == 4U &&
                   ADC_CR2_RSTCAL == 8U && ADC_CR2_SWSTART == (1U << 22U),
               "Exact F103 ADC control bits");
_Static_assert(FLASH_SR_BSY == 1U && FLASH_SR_PGERR == 4U &&
                   FLASH_SR_WRPRTERR == 16U && FLASH_CR_LOCK == 128U,
               "Exact F103 Flash command/status bits");
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
nx_result_t nx_stm32f103_adc_prepare(nx_stm32f103_adc_state_t* port,
                                     uint32_t reference_mv) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || port->active)
        return NX_ERROR_BUSY;
    port->clock_started = &g_nx_stm32f103_system.started;
    port->pin_mode = &GPIOA->CRL;
    port->pin_lock = &GPIOA->LCKR;
    port->poll_limit = 1000000U;
    port->registers = (nx_legacy_adc_registers_t*)ADC1;
    port->gate = &RCC->APB2ENR;
    port->gate_mask = RCC_APB2ENR_ADC1EN;
    port->divider = &RCC->CFGR;
    port->divider_mask = RCC_CFGR_ADCPRE;
    port->reference_mv = reference_mv;
    port->startup_delay_us = 4U;
    return nx_legacy_adc_initialize(port);
}
nx_result_t nx_stm32f103_pwm_prepare(nx_stm32f103_pwm_state_t* port,
                                     uint32_t tick_hz, uint32_t period_ticks,
                                     uint32_t duty_ticks, uint32_t inactive) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || port->running)
        return NX_ERROR_BUSY;
    port->clock_started = &g_nx_stm32f103_system.started;
    port->pin_mode = &GPIOA->CRL;
    port->pin_lock = &GPIOA->LCKR;
    port->poll_limit = 1000000U;
    port->registers = (nx_legacy_pwm_registers_t*)TIM3;
    port->gate = &RCC->APB1ENR;
    port->gate_mask = RCC_APB1ENR_TIM3EN;
    port->pin_latch = &GPIOA->ODR;
    port->pin_set = &GPIOA->BSRR;
    port->inactive = inactive;
    port->tick_hz = tick_hz;
    port->period_ticks = period_ticks;
    port->duty_ticks = duty_ticks;
    return nx_legacy_pwm_initialize(port);
}
nx_result_t nx_stm32f103_flash_prepare(nx_stm32f103_flash_state_t* port,
                                       uint32_t supply_mv) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->initialized || port->active || port->fault)
        return NX_ERROR_BUSY;
    port->registers = (nx_legacy_flash_registers_t*)FLASH;
    port->geometry = &g_nx_stm32f103_flash_geometry;
    port->memory = (volatile uint8_t*)(uintptr_t)0x08000000U;
    port->supply_mv = supply_mv;
    return nx_legacy_flash_initialize(port);
}
