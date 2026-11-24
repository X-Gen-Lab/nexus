/**
 * \file            analog.c
 * \brief           Shared cold ADC1 domain acquisition with retained rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pwm_adc/analog.h"

#define CONTROL_ADC_CLOCKS                                                     \
    (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN)

bool nx_stm32_control_adc_valid(const nx_stm32_control_adc_config_t* c) {
    if (c == NULL || c->adc == NULL || c->peers[0] == NULL ||
        c->peers[1] == NULL || c->common == NULL || c->pins == NULL ||
        c->channels == NULL || c->sample_times == NULL ||
        c->channel_count == 0U || c->channel_count > 16U ||
        c->reference_mv == 0U || c->reference_mv > 3600U ||
        c->clock_hz != NEXUS_ADC_HZ)
        return false;
#ifndef NEXUS_STM32_MODEL
    if ((c->adc != ADC1 && c->adc != ADC2 && c->adc != ADC3) ||
        c->common != ADC123_COMMON || c->adc == c->peers[0] ||
        c->adc == c->peers[1] || c->peers[0] == c->peers[1] ||
        (c->peers[0] != ADC1 && c->peers[0] != ADC2 && c->peers[0] != ADC3) ||
        (c->peers[1] != ADC1 && c->peers[1] != ADC2 && c->peers[1] != ADC3))
        return false;
#endif
    for (size_t i = 0U; i < c->channel_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->pins[i];
        uint8_t channel = c->channels[i];
        uint8_t bank = channel < 8U ? 0U : channel < 10U ? 1U : 2U;
        uint8_t number = channel < 8U
                             ? channel
                             : (uint8_t)(channel - (bank == 1U ? 8U : 10U));
        if (channel > 15U || c->sample_times[i] > 7U ||
            pin->registers == NULL || pin->snapshot == NULL ||
            pin->bank != bank || pin->pin != number)
            return false;
#ifndef NEXUS_STM32_MODEL
        if (pin->registers != (bank == 0U ? GPIOA : bank == 1U ? GPIOB : GPIOC))
            return false;
#endif
        for (size_t j = 0U; j < i; ++j)
            if (channel == c->channels[j] &&
                c->sample_times[i] != c->sample_times[j])
                return false;
    }
    return true;
}

nx_result_t nx_stm32_control_adc_admit(const nx_stm32_control_adc_t* s,
                                       const nx_stm32_control_adc_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !nx_stm32_control_adc_valid(c))
        return NX_ERROR_INVALID;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (rcc == NULL)
        return NX_ERROR_STATE;
    if (s->config != NULL ||
        (NX_AV_READ(&rcc->APB2ENR) & CONTROL_ADC_CLOCKS) != 0U ||
        (NX_AV_READ(&rcc->APB2RSTR) & RCC_APB2RSTR_ADCRST) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < c->channel_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->pins[i];
        if ((NX_AV_READ(&rcc->AHB1ENR) & (1U << pin->bank)) != 0U &&
            (NX_AV_READ(&pin->registers->LCKR) & (1U << pin->pin)) != 0U)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_control_adc_prepare(nx_stm32_control_adc_t* s,
                             const nx_stm32_control_adc_config_t* c) {
    nx_result_t result = nx_stm32_control_adc_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    for (size_t i = 0U; i < c->channel_count; ++i)
        if ((NX_AV_READ(&rcc->AHB1ENR) & (1U << c->pins[i].bank)) == 0U)
            return NX_ERROR_STATE;
    s->config = c;
    s->clocks = CONTROL_ADC_CLOCKS;
    if (!nx_stm32_av_bits(&rcc->APB2ENR, CONTROL_ADC_CLOCKS,
                          CONTROL_ADC_CLOCKS))
        return NX_ERROR_IO;
    if (((NX_AV_READ(&c->adc->CR2) | NX_AV_READ(&c->peers[0]->CR2) |
          NX_AV_READ(&c->peers[1]->CR2)) &
         ADC_CR2_ADON) != 0U)
        return NX_ERROR_BUSY;
    s->saved_common = NX_AV_READ(&c->common->CCR);
    if ((s->saved_common & (ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS |
                            ADC_CCR_TSVREFE | ADC_CCR_VBATE)) != 0U)
        return NX_ERROR_BUSY;
    s->common_touched = true;
    if (!nx_stm32_av_bits(&c->common->CCR, ADC_CCR_ADCPRE,
                          (NEXUS_CLOCK_ADC_DIV / 2U - 1U)
                              << ADC_CCR_ADCPRE_Pos) ||
        !nx_stm32_av_bits(&rcc->APB2ENR,
                          RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN, 0U))
        return NX_ERROR_IO;
    s->clocks = RCC_APB2ENR_ADC1EN;
    s->touched = true;
    bool ok = nx_stm32_av_write(&c->adc->CR2, 0U, UINT32_MAX);
    ok = nx_stm32_av_write(&c->adc->CR1, 0U, UINT32_MAX) && ok;
    for (size_t i = 0U; i < c->channel_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->pins[i];
        uint32_t field = 3U << (pin->pin * 2U);
        ok = nx_stm32_av_bits(&pin->registers->PUPDR, field, 0U) && ok;
        ok = nx_stm32_av_bits(&pin->registers->MODER, field, field) && ok;
    }
    return ok ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32_control_adc_close(nx_stm32_control_adc_t* s) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (s->config == NULL)
        return NX_SUCCESS;
    const nx_stm32_control_adc_config_t* c = s->config;
    /* A retained receipt cannot authorize MMIO after its ADC gate is lost. */
    if ((s->touched || s->common_touched) &&
        (NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & RCC_APB2ENR_ADC1EN) ==
            0U)
        return NX_ERROR_STATE;
    if (s->touched) {
        if (!nx_stm32_av_write(&c->adc->CR2, 0U, UINT32_MAX) ||
            !nx_stm32_av_write(&c->adc->CR1, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        s->touched = false;
    }
    if (s->common_touched) {
        if (!nx_stm32_av_write(&c->common->CCR, s->saved_common, UINT32_MAX))
            return NX_ERROR_IO;
        s->common_touched = false;
    }
    if (s->clocks != 0U) {
        if (!nx_stm32_av_bits(&g_nx_stm32_system.rcc->APB2ENR, s->clocks, 0U))
            return NX_ERROR_IO;
        s->clocks = 0U;
    }
    s->config = NULL;
    return NX_SUCCESS;
}

bool nx_stm32_control_adc_disjoint(const nx_stm32_control_adc_config_t* c,
                                   const void* data, size_t bytes) {
    if (!nx_stm32_av_system_disjoint(data, bytes) ||
        nx_stm32_av_overlap(data, bytes, c, sizeof(*c)) ||
        nx_stm32_av_overlap(data, bytes, c->adc, sizeof(*c->adc)) ||
        nx_stm32_av_overlap(data, bytes, c->peers[0], sizeof(*c->peers[0])) ||
        nx_stm32_av_overlap(data, bytes, c->peers[1], sizeof(*c->peers[1])) ||
        nx_stm32_av_overlap(data, bytes, c->common, sizeof(*c->common)) ||
        nx_stm32_av_overlap(data, bytes, g_nx_stm32_system.rcc,
                            sizeof(*g_nx_stm32_system.rcc)) ||
        nx_stm32_av_overlap(data, bytes, g_nx_stm32_system.timer,
                            sizeof(*g_nx_stm32_system.timer)) ||
        nx_stm32_av_overlap(data, bytes, c->channels, c->channel_count) ||
        nx_stm32_av_overlap(data, bytes, c->sample_times, c->channel_count) ||
        nx_stm32_av_overlap(data, bytes, c->pins,
                            c->channel_count * sizeof(*c->pins)))
        return false;
    for (size_t i = 0U; i < c->channel_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->pins[i];
        if (nx_stm32_av_overlap(data, bytes, pin->registers,
                                sizeof(*pin->registers)) ||
            nx_stm32_av_overlap(data, bytes, pin->snapshot,
                                sizeof(*pin->snapshot)))
            return false;
    }
    return true;
}
