/**
 * \file            devices.c
 * \brief           Exact legacy device register review and physical geometry
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include <stddef.h>
_Static_assert(offsetof(nx_legacy_adc_registers_t, control2) == 0x08U &&
                   offsetof(nx_legacy_adc_registers_t, sample2) == 0x10U &&
                   offsetof(nx_legacy_adc_registers_t, sequence3) == 0x34U &&
                   offsetof(nx_legacy_adc_registers_t, data) == 0x4cU &&
                   ADC_CTL1_ADCON == 1U && ADC_CTL1_CLB == 4U &&
                   ADC_CTL1_RSTCLB == 8U && ADC_CTL1_SWRCST == (1U << 22U),
               "Exact SDK3.0.3 GD303 ADC0 register ABI");
_Static_assert(offsetof(nx_legacy_flash_registers_t, status) ==
                       FMC_STAT0_REG_OFFSET &&
                   offsetof(nx_legacy_flash_registers_t, control) ==
                       FMC_CTL0_REG_OFFSET,
               "Exact GD303 FMC bank0 register ABI");
_Static_assert(FMC_STAT0_BUSY == 1U && FMC_STAT0_PGERR == 4U &&
                   FMC_STAT0_WPERR == 16U && FMC_CTL0_LK == 128U,
               "Exact GD303 FMC command/status bits");
_Static_assert(offsetof(nx_legacy_pwm_registers_t, compare1) == 0x34U &&
                   TIMER_CHCTL0_CH0COMCTL == 0x70U &&
                   TIMER_CHCTL0_CH0COMSEN == 8U,
               "Exact GD303 TIMER CH0 ABI");
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
nx_result_t nx_gd32f303_adc_prepare(nx_gd32f303_adc_state_t* port,
                                    uint32_t reference_mv) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || port->active)
        return NX_ERROR_BUSY;
    port->clock_started = &g_nx_gd32f303_system.started;
    port->pin_mode = &GPIO_CTL0(GPIOA);
    port->pin_lock = &GPIO_LOCK(GPIOA);
    port->poll_limit = 1000000U;
    port->registers = (nx_legacy_adc_registers_t*)(uintptr_t)ADC0;
    port->gate = &RCU_APB2EN;
    port->gate_mask = RCU_APB2EN_ADC0EN;
    port->divider = &RCU_CFG0;
    port->divider_mask = RCU_CFG0_ADCPSC | RCU_CFG0_ADCPSC_2;
    port->reference_mv = reference_mv;
    port->startup_delay_us = 2000U;
    /* The extra GD303 divider and oversampling controls are outside the
     * shared reviewed register prefix and must remain cold. */
    if ((NX_GD303_READ(&RCU_CFG1) & RCU_CFG1_ADCPSC_3) != 0U ||
        NX_GD303_READ(&ADC_OVSAMPCTL(ADC0)) != 0U)
        return NX_ERROR_BUSY;
    return nx_legacy_adc_initialize(port);
}
nx_result_t nx_gd32f303_pwm_prepare(nx_gd32f303_pwm_state_t* port,
                                    uint32_t tick_hz, uint32_t period_ticks,
                                    uint32_t duty_ticks, uint32_t inactive) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || port->running)
        return NX_ERROR_BUSY;
    port->clock_started = &g_nx_gd32f303_system.started;
    port->pin_mode = &GPIO_CTL0(GPIOA);
    port->pin_lock = &GPIO_LOCK(GPIOA);
    port->poll_limit = 1000000U;
    port->registers = (nx_legacy_pwm_registers_t*)(uintptr_t)TIMER2;
    port->gate = &RCU_APB1EN;
    port->gate_mask = RCU_APB1EN_TIMER2EN;
    port->pin_latch = &GPIO_OCTL(GPIOA);
    port->pin_set = &GPIO_BOP(GPIOA);
    port->inactive = inactive;
    port->tick_hz = tick_hz;
    port->period_ticks = period_ticks;
    port->duty_ticks = duty_ticks;
    return nx_legacy_pwm_initialize(port);
}
nx_result_t nx_gd32f303_flash_prepare(nx_gd32f303_flash_state_t* port,
                                      uint32_t supply_mv) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->initialized || port->active || port->fault)
        return NX_ERROR_BUSY;
    port->registers = (nx_legacy_flash_registers_t*)(uintptr_t)FMC;
    port->geometry = &g_nx_gd32f303_flash_geometry;
    port->memory = (volatile uint8_t*)(uintptr_t)0x08000000U;
    port->supply_mv = supply_mv;
    return nx_legacy_flash_initialize(port);
}
