/**
 * \file            adc_pair.c
 * \brief           Granted simultaneous injected-converter operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pwm_adc/adc_pair.h"
#include "stm32f4xx_ll_adc.h"

static bool object(const void* value, size_t bytes, size_t alignment) {
    const uintptr_t address = (uintptr_t)value;
    return value != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

bool nx_stm32_adc_pair_valid(const nx_stm32_adc_pair_config_t* c) {
    if (!object(c, sizeof(*c), _Alignof(nx_stm32_adc_pair_config_t)) ||
        !object(c->master, sizeof(*c->master), _Alignof(ADC_TypeDef)) ||
        !object(c->slave, sizeof(*c->slave), _Alignof(ADC_TypeDef)) ||
        !object(c->common, sizeof(*c->common), _Alignof(ADC_Common_TypeDef)) ||
        c->master == c->slave || c->channels[0] > 15U || c->channels[1] > 15U ||
        c->sample_times[0] > 7U || c->sample_times[0] != c->sample_times[1]) {
        return false;
    }
#ifndef NEXUS_STM32_MODEL
    if (c->master != ADC1 || c->slave != ADC2 || c->common != ADC123_COMMON)
        return false;
#endif
    return true;
}

/** \brief Observe the already-granted common mode without taking ownership. */
static bool simultaneous(const nx_stm32_adc_pair_config_t* c) {
    return (NX_AV_READ(&c->common->CCR) &
            (ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS)) ==
           LL_ADC_MULTI_DUAL_INJ_SIMULT;
}

static bool prepare_converter(ADC_TypeDef* adc, uint8_t channel,
                              uint8_t sample_time) {
    bool ok = nx_stm32_av_write(&adc->CR1, 0U, UINT32_MAX);
    ok = nx_stm32_av_write(&adc->CR2, 0U, UINT32_MAX) && ok;
    volatile uint32_t* sample = channel < 10U ? &adc->SMPR2 : &adc->SMPR1;
    const uint32_t shift =
        (uint32_t)(channel < 10U ? channel : channel - 10U) * 3U;
    ok =
        nx_stm32_av_bits(sample, 7U << shift, (uint32_t)sample_time << shift) &&
        ok;
    /* RM0090: a one-rank injected sequence uses JSQ4 but returns JDR1. */
    ok = nx_stm32_av_write(&adc->JSQR, (uint32_t)channel << ADC_JSQR_JSQ4_Pos,
                           UINT32_MAX) &&
         ok;
    volatile uint32_t* offsets[4] = {&adc->JOFR1, &adc->JOFR2, &adc->JOFR3,
                                     &adc->JOFR4};
    for (size_t i = 0U; i < 4U; ++i)
        ok = nx_stm32_av_write(offsets[i], 0U, UINT32_MAX) && ok;
    NX_AV_WRITE(&adc->SR, 0U);
    return NX_AV_READ(&adc->SR) == 0U && ok;
}

nx_result_t nx_stm32_adc_pair_prepare(const nx_stm32_adc_pair_config_t* c) {
    if (!nx_stm32_adc_pair_valid(c))
        return NX_ERROR_INVALID;
    if (!simultaneous(c))
        return NX_ERROR_STATE;
    if (((NX_AV_READ(&c->master->CR2) | NX_AV_READ(&c->slave->CR2)) &
         ADC_CR2_ADON) != 0U)
        return NX_ERROR_BUSY;
    bool master =
        prepare_converter(c->master, c->channels[0], c->sample_times[0]);
    bool slave =
        prepare_converter(c->slave, c->channels[1], c->sample_times[1]);
    return master && slave ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32_adc_pair_enable(const nx_stm32_adc_pair_config_t* c,
                                     uint32_t trigger_selector) {
    if (!nx_stm32_adc_pair_valid(c) ||
        (trigger_selector & ~ADC_CR2_JEXTSEL) != 0U)
        return NX_ERROR_INVALID;
    if (!simultaneous(c))
        return NX_ERROR_STATE;
    if (((NX_AV_READ(&c->master->CR2) | NX_AV_READ(&c->slave->CR2)) &
         ADC_CR2_ADON) != 0U)
        return NX_ERROR_BUSY;
    const uint32_t interrupts =
        ADC_CR1_JEOCIE | ADC_CR1_EOCIE | ADC_CR1_OVRIE | ADC_CR1_AWDIE;
    bool ok = nx_stm32_av_bits(&c->slave->CR1, interrupts, 0U);
    ok = nx_stm32_av_bits(&c->master->CR1, interrupts, 0U) && ok;
    /* Both external trigger paths stay closed until the domain publishes all
     * callback metadata; only the master will later receive JEXTEN/JEOCIE. */
    ok = nx_stm32_av_write(&c->slave->CR2, ADC_CR2_ADON, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&c->master->CR2, ADC_CR2_ADON | trigger_selector,
                           UINT32_MAX) &&
         ok;
    return ok ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32_adc_pair_read(const nx_stm32_adc_pair_config_t* c,
                                   uint16_t out[2]) {
    if (!nx_stm32_adc_pair_valid(c) ||
        !object(out, 2U * sizeof(*out), _Alignof(uint16_t)) ||
        nx_stm32_av_overlap(out, 2U * sizeof(*out), c, sizeof(*c)) ||
        nx_stm32_av_overlap(out, 2U * sizeof(*out), c->master,
                            sizeof(*c->master)) ||
        nx_stm32_av_overlap(out, 2U * sizeof(*out), c->slave,
                            sizeof(*c->slave)) ||
        nx_stm32_av_overlap(out, 2U * sizeof(*out), c->common,
                            sizeof(*c->common)))
        return NX_ERROR_INVALID;
    if (!simultaneous(c) ||
        (NX_AV_READ(&c->master->CR2) & ADC_CR2_ADON) == 0U ||
        (NX_AV_READ(&c->slave->CR2) & ADC_CR2_ADON) == 0U)
        return NX_ERROR_STATE;
    const uint32_t master = NX_AV_READ(&c->master->SR);
    const uint32_t slave = NX_AV_READ(&c->slave->SR);
    if (((master | slave) & ADC_SR_OVR) != 0U)
        return NX_ERROR_OVERFLOW;
    if ((master & slave & ADC_SR_JEOC) == 0U)
        return NX_ERROR_BUSY;
    const uint32_t pending = ADC_SR_JEOC | ADC_SR_JSTRT;
    NX_AV_WRITE(&c->master->SR, ~pending);
    NX_AV_WRITE(&c->slave->SR, ~pending);
    nx_arch_dsb();
    if (((NX_AV_READ(&c->master->SR) | NX_AV_READ(&c->slave->SR)) & pending) !=
        0U)
        return NX_ERROR_IO;
    const uint32_t first = NX_AV_READ(&c->master->JDR1);
    const uint32_t second = NX_AV_READ(&c->slave->JDR1);
    if (first > 4095U || second > 4095U)
        return NX_ERROR_INVALID;
    if (((NX_AV_READ(&c->master->SR) | NX_AV_READ(&c->slave->SR)) &
         (pending | ADC_SR_OVR)) != 0U)
        return NX_ERROR_OVERFLOW;
    out[0] = (uint16_t)first;
    out[1] = (uint16_t)second;
    return NX_SUCCESS;
}

static bool disable_converter(ADC_TypeDef* adc) {
    bool ok = nx_stm32_av_bits(
        &adc->CR1,
        ADC_CR1_JEOCIE | ADC_CR1_EOCIE | ADC_CR1_OVRIE | ADC_CR1_AWDIE, 0U);
    ok = nx_stm32_av_bits(&adc->CR2,
                          ADC_CR2_ADON | ADC_CR2_JEXTEN | ADC_CR2_EXTEN, 0U) &&
         ok;
    NX_AV_WRITE(&adc->SR, 0U);
    return NX_AV_READ(&adc->SR) == 0U && ok;
}

nx_result_t nx_stm32_adc_pair_disable(const nx_stm32_adc_pair_config_t* c) {
    if (!nx_stm32_adc_pair_valid(c))
        return NX_ERROR_INVALID;
    bool master = disable_converter(c->master);
    bool slave = disable_converter(c->slave);
    return master && slave ? NX_SUCCESS : NX_ERROR_IO;
}
