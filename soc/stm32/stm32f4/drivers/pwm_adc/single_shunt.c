/**
 * \file            single_shunt.c
 * \brief           Granted DC-link injected-converter operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pwm_adc/single_shunt.h"

static bool object(const void* value, size_t bytes, size_t alignment) {
    const uintptr_t address = (uintptr_t)value;
    return value != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

bool nx_stm32_single_shunt_valid(const nx_stm32_single_shunt_config_t* c) {
    if (!object(c, sizeof(*c), _Alignof(nx_stm32_single_shunt_config_t)) ||
        !object(c->timer, sizeof(*c->timer), _Alignof(TIM_TypeDef)) ||
        !object(c->adc, sizeof(*c->adc), _Alignof(ADC_TypeDef)) ||
        c->channel > 15U || c->sample_time > 7U || c->period_ticks < 3U ||
        c->period_ticks > 65536U || c->rearm_guard_ticks == 0U ||
        c->rearm_guard_ticks >= c->period_ticks ||
        c->maximum_lateness_ticks >= c->period_ticks ||
        (c->trigger_selector & ~ADC_CR2_JEXTSEL) != 0U) {
        return false;
    }
#ifndef NEXUS_STM32_MODEL
    if ((c->adc != ADC1 && c->adc != ADC2 && c->adc != ADC3) ||
        (c->timer != TIM1 && c->timer != TIM8) ||
        (c->timer == TIM1 && c->trigger_selector != 0U) ||
        (c->timer == TIM8 &&
         c->trigger_selector != (14U << ADC_CR2_JEXTSEL_Pos))) {
        return false;
    }
#endif
    return true;
}

/** \brief Observe only the edge carrier whose immediate compare is granted. */
static bool carrier(const nx_stm32_single_shunt_config_t* c, bool running) {
    const uint32_t control = NX_AV_READ(&c->timer->CR1);
    return (control & (TIM_CR1_CMS | TIM_CR1_DIR)) == 0U &&
           (!running || (control & TIM_CR1_CEN) != 0U) &&
           NX_AV_READ(&c->timer->ARR) == c->period_ticks - 1U &&
           (NX_AV_READ(&c->timer->CCMR2) & TIM_CCMR2_OC4PE) == 0U;
}

/** \brief Inhibit the sampling sources; a failed write keeps owner cleanup. */
static bool close_sources(const nx_stm32_single_shunt_config_t* c,
                          bool power_down) {
    bool ok = nx_stm32_av_bits(
        &c->adc->CR1,
        ADC_CR1_JEOCIE | ADC_CR1_EOCIE | ADC_CR1_AWDIE | ADC_CR1_OVRIE, 0U);
    const uint32_t mask =
        ADC_CR2_JEXTEN | ADC_CR2_EXTEN | (power_down ? ADC_CR2_ADON : 0U);
    ok = nx_stm32_av_bits(&c->adc->CR2, mask, 0U) && ok;
    return ok;
}

nx_result_t
nx_stm32_single_shunt_prepare(const nx_stm32_single_shunt_config_t* c) {
    if (!nx_stm32_single_shunt_valid(c))
        return NX_ERROR_INVALID;
    if ((NX_AV_READ(&c->adc->CR2) & ADC_CR2_ADON) != 0U)
        return NX_ERROR_BUSY;
    if (!carrier(c, false))
        return NX_ERROR_STATE;
    bool ok = nx_stm32_av_write(&c->adc->CR1, 0U, UINT32_MAX);
    ok = nx_stm32_av_write(&c->adc->CR2, 0U, UINT32_MAX) && ok;
    volatile uint32_t* sample =
        c->channel < 10U ? &c->adc->SMPR2 : &c->adc->SMPR1;
    const uint32_t shift =
        (uint32_t)(c->channel < 10U ? c->channel : c->channel - 10U) * 3U;
    ok = nx_stm32_av_bits(sample, 7U << shift,
                          (uint32_t)c->sample_time << shift) &&
         ok;
    /* RM0090: one injected rank uses JSQ4 and returns its result in JDR1. */
    ok = nx_stm32_av_write(&c->adc->JSQR,
                           (uint32_t)c->channel << ADC_JSQR_JSQ4_Pos,
                           UINT32_MAX) &&
         ok;
    volatile uint32_t* offsets[4] = {&c->adc->JOFR1, &c->adc->JOFR2,
                                     &c->adc->JOFR3, &c->adc->JOFR4};
    for (size_t i = 0U; i < 4U; ++i)
        ok = nx_stm32_av_write(offsets[i], 0U, UINT32_MAX) && ok;
    NX_AV_WRITE(&c->adc->SR, 0U);
    return NX_AV_READ(&c->adc->SR) == 0U && ok ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Require one future compare with the declared rearm margin. */
static bool future(const nx_stm32_single_shunt_config_t* c, uint32_t count,
                   uint32_t trigger) {
    return count < c->period_ticks && trigger > count &&
           trigger - count > c->rearm_guard_ticks;
}

nx_result_t nx_stm32_single_shunt_arm(const nx_stm32_single_shunt_config_t* c,
                                      uint32_t trigger_ticks) {
    if (!nx_stm32_single_shunt_valid(c) || trigger_ticks == 0U ||
        trigger_ticks >= c->period_ticks)
        return NX_ERROR_INVALID;
    if (!carrier(c, true) || (NX_AV_READ(&c->adc->CR2) & ADC_CR2_ADON) == 0U)
        return NX_ERROR_STATE;
    if ((NX_AV_READ(&c->adc->SR) & (ADC_SR_JEOC | ADC_SR_JSTRT | ADC_SR_OVR)) !=
        0U)
        return NX_ERROR_BUSY;
    const uint32_t before = NX_AV_READ(&c->timer->CNT);
    if (!future(c, before, trigger_ticks))
        return NX_ERROR_TIMEOUT;
    if (!close_sources(c, false))
        return NX_ERROR_IO;
    /* A conversion may have started between the first status read and inhibit.
     */
    if ((NX_AV_READ(&c->adc->SR) & (ADC_SR_JEOC | ADC_SR_JSTRT | ADC_SR_OVR)) !=
        0U)
        return NX_ERROR_BUSY;
    if (!nx_stm32_av_write(&c->timer->CCR4, trigger_ticks, UINT32_MAX))
        return NX_ERROR_IO;
    const uint32_t after = NX_AV_READ(&c->timer->CNT);
    if (after < before || !future(c, after, trigger_ticks))
        return NX_ERROR_TIMEOUT;
    if (!nx_stm32_av_bits(&c->adc->CR2, ADC_CR2_JEXTSEL, c->trigger_selector) ||
        !nx_stm32_av_bits(&c->adc->CR1, ADC_CR1_JEOCIE, ADC_CR1_JEOCIE) ||
        !nx_stm32_av_bits(&c->adc->CR2, ADC_CR2_JEXTEN, ADC_CR2_JEXTEN_0)) {
        (void)close_sources(c, false);
        return NX_ERROR_IO;
    }
    const uint32_t armed = NX_AV_READ(&c->timer->CNT);
    if (armed < after || armed >= trigger_ticks) {
        (void)close_sources(c, false);
        return NX_ERROR_TIMEOUT;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32_single_shunt_read(const nx_stm32_single_shunt_config_t* c,
                                       uint32_t expected_tick, uint16_t* out) {
    if (!nx_stm32_single_shunt_valid(c) || expected_tick == 0U ||
        expected_tick >= c->period_ticks ||
        !object(out, sizeof(*out), _Alignof(uint16_t)) ||
        nx_stm32_av_overlap(out, sizeof(*out), c, sizeof(*c)) ||
        nx_stm32_av_overlap(out, sizeof(*out), c->adc, sizeof(*c->adc)) ||
        nx_stm32_av_overlap(out, sizeof(*out), c->timer, sizeof(*c->timer)))
        return NX_ERROR_INVALID;
    const uint32_t status = NX_AV_READ(&c->adc->SR);
    if ((status & (ADC_SR_JEOC | ADC_SR_OVR)) == 0U)
        return NX_ERROR_BUSY;
    if (!close_sources(c, false))
        return NX_ERROR_IO;
    if ((status & ADC_SR_OVR) != 0U)
        return NX_ERROR_OVERFLOW;
    if (!carrier(c, true) || (NX_AV_READ(&c->adc->CR2) & ADC_CR2_ADON) == 0U ||
        NX_AV_READ(&c->timer->CCR4) != expected_tick)
        return NX_ERROR_STATE;
    const uint32_t before = NX_AV_READ(&c->timer->CNT);
    if (before < expected_tick || before >= c->period_ticks ||
        before - expected_tick > c->maximum_lateness_ticks)
        return NX_ERROR_TIMEOUT;
    const uint32_t pending = ADC_SR_JEOC | ADC_SR_JSTRT;
    NX_AV_WRITE(&c->adc->SR, ~pending);
    nx_arch_dsb();
    if ((NX_AV_READ(&c->adc->SR) & pending) != 0U)
        return NX_ERROR_IO;
    const uint32_t code = NX_AV_READ(&c->adc->JDR1);
    if ((NX_AV_READ(&c->adc->SR) & (pending | ADC_SR_OVR)) != 0U)
        return NX_ERROR_OVERFLOW;
    const uint32_t after = NX_AV_READ(&c->timer->CNT);
    if (after < before || after >= c->period_ticks ||
        after - expected_tick > c->maximum_lateness_ticks)
        return NX_ERROR_TIMEOUT;
    if (code > 4095U)
        return NX_ERROR_INVALID;
    *out = (uint16_t)code;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_single_shunt_disable(const nx_stm32_single_shunt_config_t* c) {
    if (!nx_stm32_single_shunt_valid(c))
        return NX_ERROR_INVALID;
    bool ok = close_sources(c, true);
    NX_AV_WRITE(&c->adc->SR, 0U);
    return NX_AV_READ(&c->adc->SR) == 0U && ok ? NX_SUCCESS : NX_ERROR_IO;
}
