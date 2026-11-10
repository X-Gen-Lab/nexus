/**
 * \file            adc.c
 * \brief           Independent ADC software shots and low-rate scans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_provider.h"
#include "stm32f4xx_ll_adc.h"

#define NX_ADC_COMMON_CLOCK                                                    \
    ((NEXUS_CLOCK_ADC_DIV / 2U - 1U) << ADC_CCR_ADCPRE_Pos)

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif
#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

nx_result_t nx_stm32_adc_sequence_validate(ADC_TypeDef* registers,
                                           const uint8_t* channels,
                                           const uint8_t* sample_times,
                                           size_t count) {
    if (registers == NULL || channels == NULL || sample_times == NULL ||
        count == 0U || count > 16U)
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < count; ++i) {
        if (channels[i] > 18U || sample_times[i] > 7U ||
            (channels[i] > 15U && (registers != ADC1 || sample_times[i] != 7U)))
            return NX_ERROR_UNSUPPORTED;
        for (size_t j = 0U; j < i; ++j) {
            if (channels[i] == channels[j] &&
                sample_times[i] != sample_times[j])
                return NX_ERROR_UNSUPPORTED;
        }
    }
    return NX_SUCCESS;
}

uint32_t nx_stm32_adc_internal_mask(const nx_stm32_adc_state_t* port) {
    uint32_t mask = 0U;
    for (size_t i = 0U; i < port->channel_count; ++i) {
        if (port->channels[i] == 16U || port->channels[i] == 17U)
            mask |= ADC_CCR_TSVREFE;
        else if (port->channels[i] == 18U)
            mask |= ADC_CCR_VBATE;
    }
    return mask;
}

nx_result_t nx_stm32_adc_internal_prepare(nx_stm32_adc_state_t* port) {
    const uint32_t mask = nx_stm32_adc_internal_mask(port);
    if (mask == 0U)
        return NX_SUCCESS;
    if ((port->common->CCR & mask) != 0U)
        return NX_ERROR_BUSY;
    /* Retain ownership before the first fallible write. Preserve unrelated
     * common paths and clock fields; only this acquired mask is released. */
    port->cold_effects |= NX_STM32_ADC_INTERNAL;
    NX_PLATFORM_REGISTER_WRITE(port->common->CCR, port->common->CCR | mask);
    nx_arch_dsb();
    return (port->common->CCR & mask) == mask ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32_adc_internal_close(nx_stm32_adc_state_t* port) {
    if ((port->cold_effects & NX_STM32_ADC_INTERNAL) == 0U)
        return NX_SUCCESS;
    const uint32_t mask = nx_stm32_adc_internal_mask(port);
    NX_PLATFORM_REGISTER_WRITE(port->common->CCR, port->common->CCR & ~mask);
    nx_arch_dsb();
    if ((port->common->CCR & mask) != 0U)
        return NX_ERROR_IO;
    port->cold_effects &= (uint8_t)~NX_STM32_ADC_INTERNAL;
    return NX_SUCCESS;
}

/** \brief Release only interface clocks temporarily acquired for observation.
 */
nx_result_t nx_stm32_adc_probe_close(nx_stm32_adc_state_t* port) {
    uint32_t clocks = 0U;
    if ((port->cold_effects & NX_STM32_ADC_PROBE1) != 0U) {
        clocks |= RCC_APB2ENR_ADC1EN;
    }
    if ((port->cold_effects & NX_STM32_ADC_PROBE2) != 0U) {
        clocks |= RCC_APB2ENR_ADC2EN;
    }
    if ((port->cold_effects & NX_STM32_ADC_PROBE3) != 0U) {
        clocks |= RCC_APB2ENR_ADC3EN;
    }
    if (clocks == 0U) {
        return NX_SUCCESS;
    }
    NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR & ~clocks);
    nx_arch_dsb();
    if ((RCC->APB2ENR & clocks) != 0U) {
        return NX_ERROR_IO;
    }
    port->cold_effects &= (uint8_t)~NX_STM32_ADC_PROBES;
    return NX_SUCCESS;
}

/** \brief Prepare the fixed domain before tasks can start any converter.
 * \note ADON cannot be assessed through a gated digital interface. Observe
 *       all three interfaces before retuning; a matching selected divider needs
 * no writes. Caller serializes all ADC preparation/start during this cold
 * phase.
 */
nx_result_t nx_stm32_adc_common_prepare(nx_stm32_adc_state_t* port) {
    if ((ADC->CCR & (ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS)) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (LL_ADC_GetCommonClock(ADC) == NX_ADC_COMMON_CLOCK) {
        return NX_SUCCESS;
    }
    const uint32_t all =
        RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN;
    uint32_t missing = all & ~RCC->APB2ENR;
    if ((missing & RCC_APB2ENR_ADC1EN) != 0U) {
        port->cold_effects |= NX_STM32_ADC_PROBE1;
    }
    if ((missing & RCC_APB2ENR_ADC2EN) != 0U) {
        port->cold_effects |= NX_STM32_ADC_PROBE2;
    }
    if ((missing & RCC_APB2ENR_ADC3EN) != 0U) {
        port->cold_effects |= NX_STM32_ADC_PROBE3;
    }
    if (missing != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR | all);
        nx_arch_dsb();
    }
    if ((RCC->APB2ENR & all) != all) {
        return NX_ERROR_IO;
    }
    nx_result_t result = NX_ERROR_BUSY;
    if (((ADC1->CR2 | ADC2->CR2 | ADC3->CR2) & ADC_CR2_ADON) == 0U) {
        LL_ADC_SetCommonClock(ADC, NX_ADC_COMMON_CLOCK);
        nx_arch_dsb();
        result = LL_ADC_GetCommonClock(ADC) == NX_ADC_COMMON_CLOCK
                     ? NX_SUCCESS
                     : NX_ERROR_IO;
    }
    nx_result_t cleanup = nx_stm32_adc_probe_close(port);
    return cleanup == NX_SUCCESS ? result : cleanup;
}

/** \brief No caller buffer survives this turn, even if device stop is refused.
 */
nx_result_t nx_stm32_adc_quiesce(nx_stm32_adc_state_t* port) {
    if (port == NULL || port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    NX_PLATFORM_REGISTER_WRITE(port->registers->CR2, 0U);
    nx_arch_dsb();
    if (port->registers->CR2 != 0U) {
        port->active = true;
        return NX_ERROR_IO;
    }
    port->registers->SR = 0U;
    port->active = false;
    return NX_SUCCESS;
}

/** \brief Initialize only independent ADC, no trigger, DMA or hidden scan task.
 */
nx_result_t nx_stm32_adc_initialize(nx_stm32_adc_state_t* port) {
    if (port == NULL || port->registers == NULL || port->common == NULL ||
        port->channels == NULL || port->sample_times == NULL ||
        port->channel_count == 0U || port->channel_count > 16U ||
        port->reference_mv == 0U || port->active) {
        return NX_ERROR_INVALID;
    }
    if (port->resolution > 3U) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t sequence =
        nx_stm32_adc_sequence_validate(port->registers, port->channels,
                                       port->sample_times, port->channel_count);
    if (sequence != NX_SUCCESS)
        return sequence;
    if (nx_stm32_adc_internal_mask(port) != 0U &&
        (port->cold_effects & NX_STM32_ADC_INTERNAL) == 0U)
        return NX_ERROR_STATE;
    if ((port->common->CCR & (ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS)) !=
            0U ||
        LL_ADC_GetCommonClock(port->common) != NX_ADC_COMMON_CLOCK) {
        return NX_ERROR_BUSY;
    }
    port->registers->CR2 = 0U;
    port->registers->CR1 = 0U;
    LL_ADC_SetResolution(port->registers,
                         (uint32_t)port->resolution << ADC_CR1_RES_Pos);
    port->registers->SQR1 = 0U;
    port->registers->SQR2 = 0U;
    port->registers->SQR3 = 0U;
    port->registers->SR = 0U;
    port->initialized = true;
    return NX_SUCCESS;
}

/** \brief Publish raw-count metadata and the nominal, uncalibrated reference.
 */
nx_result_t nx_stm32_adc_info(const void* context, nx_adc_info_t* info) {
    const nx_stm32_adc_state_t* port = context;
    if (port == NULL || !port->initialized || info == NULL) {
        return NX_ERROR_INVALID;
    }
    *info = (nx_adc_info_t){.resolution_bits =
                                (uint8_t)(12U - 2U * port->resolution),
                            .reference_mv = port->reference_mv,
                            .channel_count = port->channel_count};
    return NX_SUCCESS;
}

/** \brief Convert sequential software shots in the declared low-rate order. */
nx_result_t nx_stm32_adc_sample(void* context, uint16_t* samples,
                                size_t capacity, nx_time_us_t deadline,
                                size_t* count) {
    nx_stm32_adc_state_t* port = context;
    if (port == NULL || !port->initialized || samples == NULL ||
        count == NULL || capacity < port->channel_count) {
        return NX_ERROR_INVALID;
    }
    *count = 0U;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    /* A stopped DMA scan leaves its rank count and SCAN configuration behind.
     * Restore independent single-rank shots before enabling the converter.
     */
    port->registers->CR1 = 0U;
    LL_ADC_SetResolution(port->registers,
                         (uint32_t)port->resolution << ADC_CR1_RES_Pos);
    port->registers->SQR1 = 0U;
    port->registers->SQR2 = 0U;
    port->registers->CR2 = ADC_CR2_ADON;
    nx_arch_dsb();
    if (LL_ADC_IsEnabled(port->registers) == 0U) {
        (void)nx_stm32_adc_quiesce(port);
        return NX_ERROR_IO;
    }
    nx_time_us_t stable = nx_deadline_after(
        nx_time_now_us(), nx_stm32_adc_startup_delay_us(port));
    while (!nx_deadline_expired(stable, nx_time_now_us())) {
        NX_STM32_IO_POLL(5U, port);
    }
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0U; i < port->channel_count; ++i) {
        uint32_t channel = port->channels[i];
        volatile uint32_t* sample =
            channel < 10U ? &port->registers->SMPR2 : &port->registers->SMPR1;
        uint32_t shift = channel < 10U ? channel * 3U : (channel - 10U) * 3U;
        *sample = (*sample & ~(7U << shift)) |
                  ((uint32_t)port->sample_times[i] << shift);
        port->registers->SQR3 = channel;
        port->registers->SR = 0U;
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        LL_ADC_REG_StartConversionSWStart(port->registers);
        for (;;) {
            NX_STM32_IO_POLL(5U, port);
            if (LL_ADC_IsActiveFlag_OVR(port->registers) != 0U) {
                result = NX_ERROR_IO;
                break;
            }
            if (nx_deadline_expired(deadline, nx_time_now_us())) {
                result = NX_ERROR_TIMEOUT;
                break;
            }
            if (LL_ADC_IsActiveFlag_EOCS(port->registers) != 0U) {
                samples[(*count)++] =
                    (uint16_t)(LL_ADC_REG_ReadConversionData12(
                                   port->registers) &
                               0xFFFU);
                break;
            }
        }
        if (result != NX_SUCCESS) {
            break;
        }
    }
    nx_result_t stopped = nx_stm32_adc_quiesce(port);
    if (stopped != NX_SUCCESS) {
        return stopped;
    }
    if (result == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_adc_ops_t nx_stm32_adc_ops = {
    .info = nx_stm32_adc_info,
    .sample = nx_stm32_adc_sample,
};
