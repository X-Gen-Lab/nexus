/**
 * \file            adc_injected.c
 * \brief           Software-triggered raw injected ADC groups
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"
#include "stm32f4xx_ll_adc.h"

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif

/** \brief LL ranks encode both offset and JDR locations, not JSQR positions. */
static const uint32_t s_ranks[4] = {LL_ADC_INJ_RANK_1, LL_ADC_INJ_RANK_2,
                                    LL_ADC_INJ_RANK_3, LL_ADC_INJ_RANK_4};

/** \brief Complete one independent group before publishing raw samples. */
static nx_result_t sample(void* context, uint16_t* samples, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    nx_stm32_adc_state_t* port = context;
    if (port == NULL || !port->initialized || samples == NULL ||
        count == NULL || port->channel_count == 0U ||
        port->channel_count > 4U || capacity < port->channel_count)
        return NX_ERROR_INVALID;
    const size_t ranks = port->channel_count;
    *count = 0U;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (port->active)
        return NX_ERROR_BUSY;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    port->active = true;
    ADC_TypeDef* regs = port->registers;
    regs->CR1 = ranks > 1U ? ADC_CR1_SCAN : 0U;
    LL_ADC_SetResolution(regs, (uint32_t)port->resolution << ADC_CR1_RES_Pos);
    uint32_t sequence = (uint32_t)(ranks - 1U) << ADC_JSQR_JL_Pos;
    for (size_t i = 0U; i < ranks; ++i) {
        const uint32_t channel = port->channels[i];
        volatile uint32_t* timing = channel < 10U ? &regs->SMPR2 : &regs->SMPR1;
        const uint32_t shift =
            channel < 10U ? channel * 3U : (channel - 10U) * 3U;
        *timing = (*timing & ~(7U << shift)) |
                  ((uint32_t)port->sample_times[i] << shift);
        sequence |= channel << ((i + 4U - ranks) * 5U);
        LL_ADC_INJ_SetOffset(regs, s_ranks[i], 0U);
    }
    /* RM0090 JSQR right-aligns short groups to JSQ4. Install the whole
     * reviewed sequence with the converter off; no regular ranks are used. */
    regs->JSQR = sequence;
    regs->SR = 0U;
    regs->CR2 = ADC_CR2_ADON;
    nx_arch_dsb();
    nx_result_t result =
        LL_ADC_IsEnabled(regs) != 0U ? NX_SUCCESS : NX_ERROR_IO;
    uint16_t values[4];
    if (result == NX_SUCCESS) {
        const nx_time_us_t stable = nx_deadline_after(
            nx_time_now_us(), nx_stm32_adc_startup_delay_us(port));
        while (!nx_deadline_expired(stable, nx_time_now_us()))
            NX_STM32_IO_POLL(5U, port);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
        } else {
            LL_ADC_INJ_StartConversionSWStart(regs);
            for (;;) {
                NX_STM32_IO_POLL(5U, port);
                if (LL_ADC_IsActiveFlag_OVR(regs) != 0U) {
                    result = NX_ERROR_IO;
                    break;
                }
                if (nx_deadline_expired(deadline, nx_time_now_us())) {
                    result = NX_ERROR_TIMEOUT;
                    break;
                }
                if (LL_ADC_IsActiveFlag_JEOS(regs) != 0U) {
                    for (size_t i = 0U; i < ranks; ++i)
                        values[i] =
                            LL_ADC_INJ_ReadConversionData12(regs, s_ranks[i]);
                    break;
                }
            }
        }
    }
    nx_result_t stopped = nx_stm32_adc_quiesce(port);
    if (stopped != NX_SUCCESS)
        return stopped;
    if (result == NX_SUCCESS && nx_deadline_expired(deadline, nx_time_now_us()))
        result = NX_ERROR_TIMEOUT;
    if (result == NX_SUCCESS) {
        for (size_t i = 0U; i < ranks; ++i)
            samples[i] = values[i];
        *count = ranks;
    }
    return result;
}

/** \brief One shared injected personality with the unchanged raw ADC ABI. */
const nx_adc_ops_t nx_stm32_adc_injected_ops = {
    .info = nx_stm32_adc_info,
    .sample = sample,
};
