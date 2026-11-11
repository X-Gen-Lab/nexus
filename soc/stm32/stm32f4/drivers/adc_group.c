/**
 * \file            adc_group.c
 * \brief           Finite F407 common-ADC acquisition and full withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_adc_group.h"
#include "stm32f407_clock.h"
#include "stm32f4_adc_pins.h"
#include "stm32f4_adc_trigger.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_tim.h"

#ifndef NX_STM32_DMA_DISABLE
#define NX_STM32_DMA_DISABLE(stream, mask) ((stream)->CR &= ~(uint32_t)(mask))
#endif
#ifndef NX_STM32_ADC_GROUP_ACK
#define NX_STM32_ADC_GROUP_ACK(dma, flags) ((dma)->LIFCR = (flags))
#endif

enum { IDLE, WARM, ACQUIRING, DRAINING };
enum { CLOCKS = 1U, COMMON = 2U, VECTORS = 4U };
enum { CONVERSION_ERROR = 256U, TRANSFER_COMPLETE = 512U };
/** \brief Table69 injected sources; TIM2 is reserved by Core. */
static const nx_stm32_adc_trigger_fact_t s_injected[] = {
    {TIM1, RCC_APB2ENR_TIM1EN, 0U, 4U, true},
    {TIM1, RCC_APB2ENR_TIM1EN, 1U, 0U, true},
    {TIM3, RCC_APB1ENR_TIM3EN, 4U, 2U, false},
    {TIM3, RCC_APB1ENR_TIM3EN, 5U, 4U, false},
    {TIM4, RCC_APB1ENR_TIM4EN, 6U, 1U, false},
    {TIM4, RCC_APB1ENR_TIM4EN, 7U, 2U, false},
    {TIM4, RCC_APB1ENR_TIM4EN, 8U, 3U, false},
    {TIM4, RCC_APB1ENR_TIM4EN, 9U, 0U, false},
    {TIM5, RCC_APB1ENR_TIM5EN, 10U, 4U, false},
    {TIM5, RCC_APB1ENR_TIM5EN, 11U, 0U, false},
    {TIM8, RCC_APB2ENR_TIM8EN, 12U, 2U, true},
    {TIM8, RCC_APB2ENR_TIM8EN, 13U, 3U, true},
    {TIM8, RCC_APB2ENR_TIM8EN, 14U, 4U, true},
};
static const uint16_t s_sample_cycles[8] = {3U,  15U,  28U,  56U,
                                            84U, 112U, 144U, 480U};
static const uint16_t s_sample_halfcycles[8] = {6U,   30U,  56U,  112U,
                                                168U, 224U, 288U, 960U};
static ADC_TypeDef* const s_converters[3] = {ADC1, ADC2, ADC3};
static const uint32_t s_modes[7] = {
    LL_ADC_MULTI_DUAL_REG_SIMULT,   LL_ADC_MULTI_TRIPLE_REG_SIMULT,
    LL_ADC_MULTI_DUAL_REG_INTERL,   LL_ADC_MULTI_TRIPLE_REG_INTERL,
    LL_ADC_MULTI_INDEPENDENT,       LL_ADC_MULTI_DUAL_INJ_SIMULT,
    LL_ADC_MULTI_TRIPLE_INJ_SIMULT,
};
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Readiness is an actual live time/RCC domain, never numerical zero. */
static bool clock_live(const nx_stm32_adc_group_config_t* config) {
    return g_nx_stm32_system.started &&
           g_nx_stm32_system.timer_error == NX_SUCCESS &&
           g_nx_stm32_system.time_epoch != 0U &&
           g_nx_stm32_system.core_hz == NEXUS_CORE_HZ &&
           g_nx_stm32_system.rcc == config->rcc &&
           nx_stm32_clock_profile_matches(config->rcc, g_nx_stm32_system.flash);
}
static bool epoch_live(const nx_stm32_adc_group_state_t* state) {
    return clock_live(state->config) &&
           state->time_epoch == g_nx_stm32_system.time_epoch;
}
static bool injected(const nx_stm32_adc_group_config_t* config) {
    return config->mode >= NX_STM32_ADC_SINGLE_INJECTED;
}
static bool interleaved(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_DUAL_INTERLEAVED ||
           config->mode == NX_STM32_ADC_TRIPLE_INTERLEAVED;
}
static unsigned converters(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_SINGLE_INJECTED ? 1U
           : config->mode == NX_STM32_ADC_TRIPLE_REGULAR ||
                   config->mode == NX_STM32_ADC_TRIPLE_INTERLEAVED ||
                   config->mode == NX_STM32_ADC_TRIPLE_INJECTED
               ? 3U
               : 2U;
}
static unsigned scan_samples(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_TRIPLE_INTERLEAVED
               ? 6U
               : converters(config) * config->ranks;
}
/** \brief Mode3 stores native low-resolution interleaved byte pairs. */
static unsigned storage_bytes(const nx_stm32_adc_group_config_t* config) {
    return interleaved(config) && config->resolution >= 2U ? 1U : 2U;
}
static unsigned dma_width(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_TRIPLE_REGULAR ||
                   storage_bytes(config) == 1U
               ? 2U
               : 4U;
}
static bool timer_used(const nx_stm32_adc_group_config_t* config) {
    return !interleaved(config) && config->period_us != 0U;
}
static const nx_stm32_adc_trigger_fact_t*
trigger(const nx_stm32_adc_group_config_t* config) {
    if (!timer_used(config))
        return NULL;
    return injected(config)
               ? (config->trigger < 13U ? &s_injected[config->trigger] : NULL)
               : nx_stm32_adc_trigger_fact(config->trigger);
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Exclude writable spans from every live control/configuration fact. */
static bool independent(const nx_stm32_adc_group_state_t* state,
                        const void* data, size_t bytes, size_t alignment) {
    const nx_stm32_adc_group_config_t* config = state->config;
    if (data == NULL || bytes == 0U || (uintptr_t)data % alignment != 0U ||
        bytes > UINTPTR_MAX - (uintptr_t)data ||
        overlap(data, bytes, state, sizeof(*state)) ||
        overlap(data, bytes, config, sizeof(*config)) ||
        overlap(data, bytes, config->common, sizeof(*config->common)) ||
        overlap(data, bytes, config->rcc, sizeof(*config->rcc)) ||
        overlap(data, bytes, config->channels, scan_samples(config)) ||
        overlap(data, bytes, config->sample_times, scan_samples(config)) ||
        overlap(data, bytes, config->converters, scan_samples(config)))
        return false;
    for (unsigned i = 0U; i < 3U; ++i)
        if (overlap(data, bytes, config->adc[i], sizeof(*config->adc[i])))
            return false;
    if (timer_used(config) &&
        overlap(data, bytes, config->timer, sizeof(*config->timer)))
        return false;
    return injected(config) ||
           (!overlap(data, bytes, config->dma, sizeof(*config->dma)) &&
            !overlap(data, bytes, config->memory, sizeof(*config->memory)) &&
            !overlap(data, bytes, config->regions,
                     config->region_count * sizeof(*config->regions)));
}
static bool divider(const nx_stm32_adc_group_config_t* config, uint16_t* psc,
                    uint16_t* arr) {
    if (!timer_used(config))
        return true;
    const nx_stm32_adc_trigger_fact_t* f = trigger(config);
    if (f == NULL || config->timer == NULL ||
        config->timer_clock_hz != nx_stm32_adc_trigger_clock(f))
        return false;
    uint64_t numerator = (uint64_t)config->period_us * config->timer_clock_hz;
    if (numerator % 1000000U != 0U)
        return false;
    uint64_t ticks = numerator / 1000000U;
    if (ticks == 0U || ticks > UINT64_C(4294967296))
        return false;
    for (uint32_t divide = (uint32_t)((ticks - 1U) / 65536U + 1U);
         divide <= 65536U; ++divide)
        if (ticks % divide == 0U && ticks / divide <= 65536U) {
            *psc = (uint16_t)(divide - 1U);
            *arr = (uint16_t)(ticks / divide - 1U);
            return true;
        }
    return false;
}
/** \brief Check silicon, sequence and conversion budgets before any effect. */
static bool facts(const nx_stm32_adc_group_config_t* config) {
    if (config == NULL || config->rcc == NULL || config->common == NULL ||
        config->channels == NULL || config->sample_times == NULL ||
        config->converters == NULL || (unsigned)config->mode >= 7U ||
        config->resolution > 3U || config->ranks == 0U ||
        config->ranks > (injected(config) ? 4U : 16U) ||
        (interleaved(config) && config->ranks != 1U) ||
        config->priority > 15U || config->adc_clock_hz != NEXUS_ADC_HZ ||
        config->drain_timeout_us == 0U || config->delay_cycles < 5U ||
        config->delay_cycles > 20U ||
        (!injected(config) && !interleaved(config) &&
         config->period_us == 0U) ||
        (interleaved(config) && config->period_us != 0U) ||
        (timer_used(config) && config->trigger_identity == 0U) ||
        (config->watchdog &&
         (config->lower > config->upper ||
          config->upper >= (1U << (12U - 2U * config->resolution)))))
        return false;
    for (unsigned i = 0U; i < 3U; ++i)
        if (config->adc[i] == NULL)
            return false;
    if (!injected(config) &&
        (config->dma == NULL || config->memory == NULL ||
         config->regions == NULL || config->region_count == 0U ||
         config->region_count > SIZE_MAX / sizeof(*config->regions)))
        return false;
    const unsigned count = converters(config);
    if (config->mode == NX_STM32_ADC_TRIPLE_INTERLEAVED)
        for (unsigned i = 0U; i < 3U; ++i)
            if (config->channels[i + 3U] != config->channels[i] ||
                config->sample_times[i + 3U] != config->sample_times[i] ||
                config->converters[i + 3U] != config->converters[i])
                return false;
    uint32_t cycles = 0U;
    for (unsigned rank = 0U; rank < config->ranks; ++rank) {
        cycles +=
            s_sample_cycles[config->sample_times[(size_t)rank * count] & 7U] +
            12U - 2U * config->resolution;
        for (unsigned i = 0U; i < count; ++i) {
            unsigned index = rank * count + i;
            uint8_t channel = config->channels[index];
            if (channel > 15U || config->sample_times[index] > 7U ||
                config->converters[index] != i + 1U ||
                config->sample_times[index] !=
                    config->sample_times[(size_t)rank * count])
                return false;
            for (unsigned prior = 0U; prior < rank; ++prior)
                if (config->channels[prior * count + i] == channel &&
                    config->sample_times[prior * count + i] !=
                        config->sample_times[index])
                    return false;
            if (interleaved(config)) {
                if (channel != config->channels[0] ||
                    (count == 3U && channel > 3U &&
                     (channel < 10U || channel > 13U)))
                    return false;
            } else if (count > 1U) {
                nx_stm32_adc_pad_t pad, previous;
                if (!nx_stm32_adc_pad(s_converters[i], channel, &pad))
                    return false;
                for (unsigned other = 0U; other < i; ++other) {
                    if (!nx_stm32_adc_pad(
                            s_converters[other],
                            config->channels[rank * count + other], &previous))
                        return false;
                    if (pad.gpio == previous.gpio && pad.pin == previous.pin)
                        return false;
                }
            }
        }
    }
    return !timer_used(config) ||
           (uint64_t)config->period_us * config->adc_clock_hz >
               (uint64_t)cycles * 1000000U;
}
static bool foreign_vectors(const nx_stm32_adc_group_config_t* config) {
#ifndef NEXUS_STM32_MODEL
    if (NVIC_GetEnableIRQ(ADC_IRQn) || NVIC_GetPendingIRQ(ADC_IRQn) ||
        NVIC_GetActive(ADC_IRQn))
        return true;
    if (!injected(config) && (NVIC_GetEnableIRQ(DMA2_Stream0_IRQn) ||
                              NVIC_GetPendingIRQ(DMA2_Stream0_IRQn) ||
                              NVIC_GetActive(DMA2_Stream0_IRQn)))
        return true;
#else
    (void)config;
#endif
    return false;
}
nx_result_t
nx_stm32_adc_group_prepare(nx_stm32_adc_group_state_t* state,
                           const nx_stm32_adc_group_config_t* config) {
    if (state == NULL || !facts(config))
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->config != NULL || state->active != NULL || state->effects != 0U)
        return NX_ERROR_BUSY;
    if (!clock_live(config))
        return NX_ERROR_STATE;
    uint16_t psc = 0U, arr = 0U;
    if (!divider(config, &psc, &arr))
        return NX_ERROR_UNSUPPORTED;
#ifndef NEXUS_STM32_MODEL
    if (config->adc[0] != ADC1 || config->adc[1] != ADC2 ||
        config->adc[2] != ADC3 || config->common != ADC123_COMMON ||
        config->rcc != RCC ||
        (timer_used(config) && config->timer != trigger(config)->registers) ||
        (!injected(config) &&
         (config->dma != DMA2 || config->memory != DMA2_Stream0)))
        return NX_ERROR_UNSUPPORTED;
    if (!nx_stm32_clock_profile_matches(config->rcc, NULL))
        return NX_ERROR_STATE;
#endif
    if (foreign_vectors(config) ||
        (config->common->CCR & (ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS)) !=
            0U ||
        (timer_used(config) &&
         ((config->timer->CR1 & TIM_CR1_CEN) != 0U ||
          config->timer->CCER != 0U || config->timer->DIER != 0U)) ||
        (!injected(config) && (config->memory->CR & DMA_SxCR_EN) != 0U))
        return NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 3U; ++i)
        if ((config->adc[i]->CR2 &
             (ADC_CR2_ADON | ADC_CR2_EXTEN | ADC_CR2_JEXTEN | ADC_CR2_DMA |
              ADC_CR2_DDS)) != 0U ||
            (config->adc[i]->CR1 & (ADC_CR1_EOCIE | ADC_CR1_JEOCIE |
                                    ADC_CR1_OVRIE | ADC_CR1_AWDIE)) != 0U)
            return NX_ERROR_BUSY;
    state->config = config;
    state->prescaler = psc;
    state->period = arr;
    uint32_t gates =
        RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN;
    const nx_stm32_adc_trigger_fact_t* f = trigger(config);
    if (f != NULL && f->apb2)
        gates |= f->gate;
    state->gates_added = gates & ~config->rcc->APB2ENR;
    state->effects = CLOCKS;
    config->rcc->APB2ENR |= gates;
    /* A high receipt bit records APB1 without another per-instance word. */
    if (f != NULL && !f->apb2 && (config->rcc->APB1ENR & f->gate) == 0U) {
        state->gates_added |= UINT32_C(0x80000000);
        config->rcc->APB1ENR |= f->gate;
    }
    nx_arch_dsb();
    if ((config->rcc->APB2ENR & gates) != gates ||
        (f != NULL && !f->apb2 && (config->rcc->APB1ENR & f->gate) == 0U))
        return NX_ERROR_IO;
    /* Gated ADC registers can read as zero. Recheck once the probe clocks
     * are observed, before changing a common field or stopping a converter. */
    state->saved_common = config->common->CCR;
    if ((config->common->CCR & (ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS)) !=
            0U ||
        (timer_used(config) &&
         ((config->timer->CR1 & TIM_CR1_CEN) != 0U ||
          config->timer->CCER != 0U || config->timer->DIER != 0U)))
        return NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 3U; ++i)
        if ((config->adc[i]->CR2 &
             (ADC_CR2_ADON | ADC_CR2_EXTEN | ADC_CR2_JEXTEN | ADC_CR2_DMA |
              ADC_CR2_DDS)) != 0U ||
            (config->adc[i]->CR1 & (ADC_CR1_EOCIE | ADC_CR1_JEOCIE |
                                    ADC_CR1_OVRIE | ADC_CR1_AWDIE)) != 0U)
            return NX_ERROR_BUSY;
    for (unsigned rank = 0U; rank < config->ranks; ++rank)
        for (unsigned i = 0U; i < converters(config); ++i) {
            uint8_t channel = config->channels[rank * converters(config) + i];
            nx_result_t result = nx_stm32_adc_pin(s_converters[i], channel);
            if (result != NX_SUCCESS)
                return result;
        }
    nx_arch_dsb();
    for (unsigned rank = 0U; rank < config->ranks; ++rank)
        for (unsigned i = 0U; i < converters(config); ++i) {
            nx_stm32_adc_pad_t pad;
            if (!nx_stm32_adc_pad(
                    s_converters[i],
                    config->channels[rank * converters(config) + i], &pad))
                return NX_ERROR_INVALID;
            uint32_t shift = (uint32_t)pad.pin * 2U;
            if ((pad.gpio->MODER & (3U << shift)) != (3U << shift) ||
                (pad.gpio->PUPDR & (3U << shift)) != 0U)
                return NX_ERROR_IO;
        }
    state->initialized = true;
    state->time_epoch = g_nx_stm32_system.time_epoch;
    state->closing = state->fault = false;
    state->phase = IDLE;
    return NX_SUCCESS;
}
/** \brief Bounded observations never touch a borrowed request or its samples.
 */
static uint32_t observed(const nx_stm32_adc_group_config_t* config) {
    uint32_t events = 0U;
    for (unsigned i = 0U; i < converters(config); ++i) {
        uint32_t sr = config->adc[i]->SR;
        if ((sr & (ADC_SR_OVR | (config->watchdog ? ADC_SR_AWD : 0U))) != 0U)
            events |= CONVERSION_ERROR;
        if ((sr & ADC_SR_JEOC) != 0U)
            events |= 1U << i;
    }
    if (!injected(config)) {
        uint32_t flags = config->dma->LISR;
        if ((flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)) != 0U)
            events |= CONVERSION_ERROR;
        if ((flags & DMA_LISR_TCIF0) != 0U)
            events |= TRANSFER_COMPLETE;
    }
    return events;
}
/** \brief Bound post-completion sampling before a task can run its full join.
 */
static void gate(const nx_stm32_adc_group_config_t* config) {
    if (timer_used(config))
        LL_TIM_DisableCounter(config->timer);
    for (unsigned i = 0U; i < converters(config); ++i) {
        config->adc[i]->CR1 &= ~(uint32_t)(ADC_CR1_EOCIE | ADC_CR1_JEOCIE |
                                           ADC_CR1_OVRIE | ADC_CR1_AWDIE);
        config->adc[i]->CR2 &=
            ~(uint32_t)(ADC_CR2_ADON | ADC_CR2_EXTEN | ADC_CR2_JEXTEN |
                        ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_CONT |
                        ADC_CR2_SWSTART | ADC_CR2_JSWSTART);
    }
    config->common->CCR &= ~(uint32_t)(ADC_CCR_DMA | ADC_CCR_DDS);
    nx_arch_dsb();
}
void nx_stm32_adc_group_irq(const nx_adc_group_port_t* port) {
    nx_stm32_adc_group_state_t* state = port == NULL ? NULL : port->context;
    if (state == NULL || !state->initialized || state->config == NULL ||
        state->phase == IDLE)
        return;
    state->events |= observed(state->config);
    if (!epoch_live(state)) {
        state->events |= CONVERSION_ERROR;
        gate(state->config);
        return;
    }
    /* Equal-priority vectors cannot nest one another. Mask level publishers
     * after latching, then let the unique task owner perform the full drain. */
    for (unsigned i = 0U; i < converters(state->config); ++i)
        state->config->adc[i]->CR1 &=
            ~(uint32_t)(ADC_CR1_JEOCIE | ADC_CR1_OVRIE | ADC_CR1_AWDIE);
    if (!injected(state->config)) {
        state->config->memory->CR &=
            ~(uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
        state->config->memory->FCR &= ~(uint32_t)DMA_SxFCR_FEIE;
    }
    uint32_t completed = (1U << converters(state->config)) - 1U;
    if ((state->events & (CONVERSION_ERROR | TRANSFER_COMPLETE)) != 0U ||
        (injected(state->config) && (state->events & completed) == completed))
        gate(state->config);
}
static void begin_drain(nx_stm32_adc_group_state_t* state, nx_result_t result) {
    if (state->phase == DRAINING)
        return;
    state->terminal = result;
    state->fault |= result == NX_ERROR_IO;
    state->phase = DRAINING;
    nx_time_us_t now = nx_time_now_us();
    if (nx_deadline_finite_after(now, state->config->drain_timeout_us,
                                 &state->drain_deadline) != NX_SUCCESS)
        state->drain_deadline = now == NX_DEADLINE_NEVER ? now - 1U : now;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
}
/** \brief Program disabled converters, then start the post-store warmup. */
static nx_result_t prime(nx_stm32_adc_group_state_t* state) {
    const nx_stm32_adc_group_config_t* config = state->config;
    const unsigned count = converters(config);
    if (!injected(config))
        NX_STM32_ADC_GROUP_ACK(config->dma, UINT32_C(0x3d));
    state->effects |= COMMON;
    /* Interrupted interleave must pass through independent MULTI=0. */
    LL_ADC_SetMultimode(config->common, LL_ADC_MULTI_INDEPENDENT);
    nx_arch_dsb();
    if ((config->common->CCR & ADC_CCR_MULTI) != 0U)
        return NX_ERROR_IO;
    uint32_t common =
        state->saved_common &
        ~(uint32_t)(ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS | ADC_CCR_DELAY |
                    ADC_CCR_TSVREFE | ADC_CCR_VBATE | ADC_CCR_ADCPRE);
    common |= s_modes[config->mode] |
              ((uint32_t)(config->delay_cycles - 5U) << 8U) |
              ((NEXUS_CLOCK_ADC_DIV / 2U - 1U) << ADC_CCR_ADCPRE_Pos);
    if (!injected(config))
        common |= (config->mode == NX_STM32_ADC_TRIPLE_REGULAR ? ADC_CCR_DMA_0
                   : storage_bytes(config) == 1U               ? ADC_CCR_DMA
                                                 : ADC_CCR_DMA_1) |
                  ADC_CCR_DDS;
    config->common->CCR = common;
    for (unsigned i = 0U; i < count; ++i) {
        ADC_TypeDef* adc = config->adc[i];
        adc->CR2 = 0U;
        uint32_t control = (config->ranks > 1U ? ADC_CR1_SCAN : 0U) |
                           ADC_CR1_OVRIE |
                           ((uint32_t)config->resolution << ADC_CR1_RES_Pos);
        if (injected(config))
            control |= ADC_CR1_JEOCIE;
        if (config->watchdog) {
            unsigned shift = 2U * config->resolution;
            LL_ADC_SetAnalogWDThresholds(adc, LL_ADC_AWD_THRESHOLD_LOW,
                                         (uint32_t)config->lower << shift);
            LL_ADC_SetAnalogWDThresholds(adc, LL_ADC_AWD_THRESHOLD_HIGH,
                                         (uint32_t)config->upper << shift);
            control |= ADC_CR1_AWDIE |
                       (injected(config) ? ADC_CR1_JAWDEN : ADC_CR1_AWDEN);
        }
        adc->CR1 = control;
        adc->SMPR1 = adc->SMPR2 = 0U;
        adc->SQR1 = ((uint32_t)config->ranks - 1U) << ADC_SQR1_L_Pos;
        adc->SQR2 = adc->SQR3 = 0U;
        adc->JSQR = ((uint32_t)config->ranks - 1U) << ADC_JSQR_JL_Pos;
        adc->JOFR1 = adc->JOFR2 = adc->JOFR3 = adc->JOFR4 = 0U;
        for (unsigned rank = 0U; rank < config->ranks; ++rank) {
            unsigned index = rank * count + i;
            uint32_t channel = config->channels[index];
            uint32_t timing = config->sample_times[index];
            if (channel < 10U)
                adc->SMPR2 |= timing << (channel * 3U);
            else
                adc->SMPR1 |= timing << ((channel - 10U) * 3U);
            if (injected(config))
                adc->JSQR |= channel << ((rank + 4U - config->ranks) * 5U);
            else if (rank < 6U)
                adc->SQR3 |= channel << (rank * 5U);
            else if (rank < 12U)
                adc->SQR2 |= channel << ((rank - 6U) * 5U);
            else
                adc->SQR1 |= channel << ((rank - 12U) * 5U);
        }
        nx_arch_dsb();
        if (adc->CR1 != control || adc->CR2 != 0U)
            return NX_ERROR_IO;
        adc->SR = 0U;
        LL_ADC_Enable(adc);
    }
    nx_arch_dsb();
    for (unsigned i = 0U; i < count; ++i)
        if (!LL_ADC_IsEnabled(config->adc[i]))
            return NX_ERROR_IO;
    if (config->common->CCR != common)
        return NX_ERROR_IO;
    return nx_deadline_finite_after(nx_time_now_us(), 4U, &state->warm_until);
}
/** \brief Arm memory, consumers, then the source; setup UG cannot sample. */
static nx_result_t arm(nx_stm32_adc_group_state_t* state) {
    const nx_stm32_adc_group_config_t* config = state->config;
    const nx_stm32_adc_trigger_fact_t* f = trigger(config);
    if (f != NULL) {
        nx_stm32_adc_trigger_program(config->timer, f, state->prescaler,
                                     state->period);
        if (injected(config))
            config->timer->CR1 |= TIM_CR1_OPM;
    }
    if (!injected(config)) {
        DMA_Stream_TypeDef* memory = config->memory;
        NX_STM32_ADC_GROUP_ACK(config->dma, UINT32_C(0x3d));
        memory->CR =
            DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE |
            (dma_width(config) == 4U ? DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1
                                     : DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0);
        memory->FCR = DMA_SxFCR_FEIE;
        memory->PAR = (uint32_t)(uintptr_t)&config->common->CDR;
        memory->M0AR = (uint32_t)(uintptr_t)state->active->frames;
        memory->M1AR = 0U;
        memory->NDTR = (uint32_t)(state->active->bytes / dma_width(config));
        memory->CR |= DMA_SxCR_EN;
        nx_arch_dsb();
        if ((memory->CR & DMA_SxCR_EN) == 0U ||
            memory->M0AR != (uint32_t)(uintptr_t)state->active->frames ||
            memory->PAR != (uint32_t)(uintptr_t)&config->common->CDR ||
            memory->NDTR != state->active->bytes / dma_width(config))
            return NX_ERROR_IO;
    }
    state->effects |= VECTORS;
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(ADC_IRQn);
    NVIC_SetPriority(ADC_IRQn, config->priority);
    NVIC_EnableIRQ(ADC_IRQn);
    if (!injected(config)) {
        NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
        NVIC_SetPriority(DMA2_Stream0_IRQn, config->priority);
        NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    }
#endif
    if (f != NULL) {
        uint32_t external =
            injected(config) ? ((uint32_t)f->selection << ADC_CR2_JEXTSEL_Pos) |
                                   ADC_CR2_JEXTEN_0
                             : ((uint32_t)f->selection << ADC_CR2_EXTSEL_Pos) |
                                   ADC_CR2_EXTEN_0;
        config->adc[0]->CR2 |= external;
        nx_arch_dsb();
        if ((config->adc[0]->CR2 & external) != external)
            return NX_ERROR_IO;
        LL_TIM_EnableCounter(config->timer);
    } else if (injected(config)) {
        LL_ADC_INJ_StartConversionSWStart(config->adc[0]);
    } else {
        for (unsigned i = 0U; i < converters(config); ++i)
            LL_ADC_REG_SetContinuousMode(config->adc[i],
                                         LL_ADC_REG_CONV_CONTINUOUS);
        LL_ADC_REG_StartConversionSWStart(config->adc[0]);
    }
    nx_arch_dsb();
    return f != NULL && (config->timer->CR1 & TIM_CR1_CEN) == 0U ? NX_ERROR_IO
                                                                 : NX_SUCCESS;
}
static nx_result_t submit(void* context, nx_adc_group_request_t* request) {
    nx_stm32_adc_group_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->initialized || state->closing || state->fault)
        return NX_ERROR_STATE;
    if (!epoch_live(state))
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    const nx_stm32_adc_group_config_t* config = state->config;
    size_t stride = (size_t)scan_samples(config) * storage_bytes(config);
    if (!independent(state, request, sizeof(*request),
                     _Alignof(nx_adc_group_request_t)) ||
        request->frame_count == 0U ||
        request->frame_count > SIZE_MAX / stride ||
        request->bytes != request->frame_count * stride ||
        request->base.deadline == NX_DEADLINE_NEVER ||
        !independent(state, request->frames, request->bytes,
                     injected(config) ? _Alignof(uint16_t)
                                      : dma_width(config)) ||
        overlap(request, sizeof(*request), request->frames, request->bytes) ||
        (injected(config) ? request->frame_count != 1U
                          : request->bytes / dma_width(config) > 65535U))
        return NX_ERROR_INVALID;
    if (!injected(config)) {
        nx_result_t result = nx_dma_buffer_validate(
            config->regions, config->region_count, request->frames,
            request->bytes, dma_width(config), NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
#ifndef NEXUS_STM32_MODEL
        if ((uintptr_t)request->frames > UINT32_MAX ||
            request->bytes > UINT32_MAX - (uintptr_t)request->frames)
            return NX_ERROR_INVALID;
#endif
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    state->active = request;
    state->phase = WARM;
    state->terminal = NX_SUCCESS;
    state->events = 0U;
    result = prime(state);
    if (result != NX_SUCCESS)
        begin_drain(state, result);
    return NX_SUCCESS;
}
/** \brief Observe every final-access source before clearing hardware pointers.
 */
static bool withdraw(nx_stm32_adc_group_state_t* state) {
    const nx_stm32_adc_group_config_t* config = state->config;
    state->events |= observed(config);
    gate(config);
    if (!injected(config)) {
        NX_STM32_DMA_DISABLE(config->memory, DMA_SxCR_EN | DMA_SxCR_TCIE |
                                                 DMA_SxCR_HTIE | DMA_SxCR_TEIE |
                                                 DMA_SxCR_DMEIE);
        config->memory->FCR &= ~(uint32_t)DMA_SxFCR_FEIE;
    }
    nx_arch_dsb();
    if ((timer_used(config) && (config->timer->CR1 & TIM_CR1_CEN) != 0U) ||
        (config->common->CCR & (ADC_CCR_DMA | ADC_CCR_DDS)) != 0U)
        return false;
    for (unsigned i = 0U; i < converters(config); ++i)
        if ((config->adc[i]->CR2 &
             (ADC_CR2_ADON | ADC_CR2_EXTEN | ADC_CR2_JEXTEN | ADC_CR2_DMA |
              ADC_CR2_DDS | ADC_CR2_CONT)) != 0U ||
            (config->adc[i]->CR1 & (ADC_CR1_EOCIE | ADC_CR1_JEOCIE |
                                    ADC_CR1_OVRIE | ADC_CR1_AWDIE)) != 0U)
            return false;
    if (!injected(config) &&
        ((config->memory->CR & (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                DMA_SxCR_TEIE | DMA_SxCR_DMEIE)) != 0U ||
         (config->memory->FCR & DMA_SxFCR_FEIE) != 0U))
        return false;
#ifndef NEXUS_STM32_MODEL
    if ((state->effects & VECTORS) != 0U) {
        NVIC_DisableIRQ(ADC_IRQn);
        if (!injected(config))
            NVIC_DisableIRQ(DMA2_Stream0_IRQn);
        nx_arch_dsb();
        if (NVIC_GetEnableIRQ(ADC_IRQn) || NVIC_GetActive(ADC_IRQn) ||
            (!injected(config) && (NVIC_GetEnableIRQ(DMA2_Stream0_IRQn) ||
                                   NVIC_GetActive(DMA2_Stream0_IRQn))))
            return false;
        NVIC_ClearPendingIRQ(ADC_IRQn);
        if (!injected(config))
            NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
        nx_arch_dsb();
        if (NVIC_GetPendingIRQ(ADC_IRQn) ||
            (!injected(config) && NVIC_GetPendingIRQ(DMA2_Stream0_IRQn)))
            return false;
    }
#endif
    state->effects &= (uint8_t)~VECTORS;
    if (!injected(config)) {
        uint32_t remaining = config->memory->NDTR;
        state->events |= observed(config);
        NX_STM32_ADC_GROUP_ACK(config->dma, UINT32_C(0x3d));
        nx_arch_dsb();
        state->events |= observed(config);
        if (remaining != config->memory->NDTR)
            return false;
        config->memory->PAR = config->memory->M0AR = config->memory->M1AR = 0U;
        nx_arch_dsb();
        if (config->memory->PAR || config->memory->M0AR ||
            config->memory->M1AR || config->memory->NDTR != remaining)
            return false;
    }
    LL_ADC_SetMultimode(config->common, LL_ADC_MULTI_INDEPENDENT);
    nx_arch_dsb();
    return (config->common->CCR & ADC_CCR_MULTI) == 0U;
}
static nx_result_t service(void* context) {
    nx_stm32_adc_group_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->initialized)
        return NX_ERROR_STATE;
    if (state->active == NULL)
        return NX_SUCCESS;
    const nx_stm32_adc_group_config_t* config = state->config;
    const bool live = epoch_live(state);
    nx_time_us_t now = live ? nx_time_now_us() : 0U;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->events |= observed(config);
    if (state->phase != DRAINING) {
        if (!live)
            begin_drain(state, NX_ERROR_STATE);
        else if ((state->events & CONVERSION_ERROR) != 0U)
            begin_drain(state, NX_ERROR_IO);
        else if (nx_deadline_expired(state->active->base.deadline, now))
            begin_drain(state, NX_ERROR_TIMEOUT);
        else if (state->phase == WARM &&
                 nx_deadline_expired(state->warm_until, now)) {
            nx_result_t result = arm(state);
            if (result == NX_SUCCESS)
                state->phase = ACQUIRING;
            else
                begin_drain(state, result);
        } else if (state->phase == ACQUIRING) {
            bool complete =
                injected(config)
                    ? (state->events & ((1U << converters(config)) - 1U)) ==
                          ((1U << converters(config)) - 1U)
                    : (state->events & TRANSFER_COMPLETE) != 0U;
            if (complete)
                begin_drain(state,
                            !injected(config) && config->memory->NDTR != 0U
                                ? NX_ERROR_IO
                                : NX_SUCCESS);
        }
    }
    nx_arch_irq_restore(saved);
    if (state->phase != DRAINING)
        return NX_SUCCESS;
    if (!withdraw(state)) {
        if (!live ||
            nx_deadline_expired(state->drain_deadline, nx_time_now_us())) {
            state->fault = true;
            (void)nx_request_transition(&state->active->base,
                                        NX_REQUEST_QUARANTINED);
            return live ? NX_ERROR_IO : NX_ERROR_STATE;
        }
        return NX_ERROR_BUSY;
    }
    state->events |= observed(config);
    nx_result_t result = state->terminal;
    if (result == NX_SUCCESS && !epoch_live(state))
        result = NX_ERROR_STATE;
    if (result == NX_SUCCESS && (state->events & CONVERSION_ERROR) != 0U)
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS &&
        nx_deadline_expired(state->active->base.deadline, nx_time_now_us()))
        result = NX_ERROR_TIMEOUT;
    if (result == NX_SUCCESS && injected(config)) {
        static const uint32_t ranks[4] = {LL_ADC_INJ_RANK_1, LL_ADC_INJ_RANK_2,
                                          LL_ADC_INJ_RANK_3, LL_ADC_INJ_RANK_4};
        uint16_t* data = state->active->frames;
        const unsigned count = converters(config);
        for (unsigned rank = 0U; rank < config->ranks; ++rank)
            for (unsigned i = 0U; i < count; ++i)
                data[rank * count + i] = LL_ADC_INJ_ReadConversionData12(
                    config->adc[i], ranks[rank]);
    }
    if (result == NX_SUCCESS && !epoch_live(state))
        result = NX_ERROR_STATE;
    state->fault |= result == NX_ERROR_IO;
    nx_adc_group_request_t* request = state->active;
    size_t frames = result == NX_SUCCESS ? request->frame_count : 0U;
    state->active = NULL;
    state->phase = IDLE;
    nx_request_settle(&request->base, result, frames);
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_adc_group_request_t* request) {
    nx_stm32_adc_group_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->initialized || request == NULL || state->active != request)
        return NX_ERROR_STATE;
    begin_drain(state, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32_adc_group_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->initialized)
        return NX_ERROR_STATE;
    if (state->active != NULL) {
        begin_drain(state, NX_ERROR_CANCELLED);
        nx_result_t result = service(state);
        if (result != NX_SUCCESS)
            return result;
    }
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    state->fault = false;
    return NX_SUCCESS;
}
static nx_result_t info(const void* context, nx_adc_group_info_t* output) {
    const nx_stm32_adc_group_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->initialized)
        return NX_ERROR_STATE;
    if (!independent(state, output, sizeof(*output),
                     _Alignof(nx_adc_group_info_t)))
        return NX_ERROR_INVALID;
    if (state->active != NULL &&
        (overlap(output, sizeof(*output), state->active,
                 sizeof(*state->active)) ||
         overlap(output, sizeof(*output), state->active->frames,
                 state->active->bytes)))
        return NX_ERROR_INVALID;
    const nx_stm32_adc_group_config_t* config = state->config;
    unsigned count = scan_samples(config);
    const size_t frame_bytes = (size_t)count * storage_bytes(config);
    /* Prepare establishes this immutable, nonzero frame divisor. */
    if (frame_bytes == 0U)
        return NX_ERROR_STATE;
    nx_adc_group_info_t value = {0};
    value.format = (nx_adc_format_t){
        .sample_stride = storage_bytes(config),
        .scan_stride = frame_bytes,
        .trigger_identity = timer_used(config) ? config->trigger_identity : 0U,
        .channel_count = (uint16_t)count,
        .storage_bytes = (uint8_t)storage_bytes(config),
        .significant_bits = (uint8_t)(12U - 2U * config->resolution),
        .encoding = NX_ADC_UNSIGNED,
        .byte_order = NX_ADC_LITTLE_ENDIAN,
        .group = injected(config) ? NX_ADC_INJECTED : NX_ADC_REGULAR,
        .trigger = timer_used(config) ? NX_ADC_TRIGGER_TIMER
                                      : NX_ADC_TRIGGER_SOFTWARE};
    value.channels = config->channels;
    value.sample_times = config->sample_times;
    value.converters = config->converters;
    value.sample_halfcycles = s_sample_halfcycles;
    value.sample_code_count = 8U;
    value.adc_clock_hz = config->adc_clock_hz;
    if (!interleaved(config))
        for (unsigned rank = 0U; rank < config->ranks; ++rank)
            value.physical_scan_halfcycles +=
                2U *
                (s_sample_cycles[config->sample_times[(size_t)rank *
                                                      converters(config)]] +
                 12U - 2U * config->resolution);
    value.interleave_delay_cycles =
        interleaved(config) ? config->delay_cycles : 0U;
    value.buffer_alignment =
        (uint8_t)(injected(config) ? 2U : dma_width(config));
    value.timer_period_us = timer_used(config) ? config->period_us : 0U;
    value.max_frames = injected(config)
                           ? 1U
                           : (size_t)65535U * dma_width(config) / frame_bytes;
    value.physical_ranks = (uint16_t)scan_samples(config);
    value.active = state->active != NULL;
    value.fault = state->fault;
    value.quarantined =
        state->active != NULL &&
        nx_request_state(&state->active->base) == NX_REQUEST_QUARANTINED;
    *output = value;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_adc_group_close(nx_stm32_adc_group_state_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL)
        return state->effects == 0U ? NX_SUCCESS : NX_ERROR_STATE;
    state->closing = true;
    if (state->initialized) {
        nx_result_t result = stop(state);
        if (result != NX_SUCCESS)
            return result;
    }
    const nx_stm32_adc_group_config_t* config = state->config;
    if ((state->effects & COMMON) != 0U) {
        config->common->CCR = state->saved_common;
        nx_arch_dsb();
        if (config->common->CCR != state->saved_common)
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~COMMON;
    }
    const nx_stm32_adc_trigger_fact_t* f = trigger(config);
    if ((state->gates_added & UINT32_C(0x80000000)) != 0U) {
        config->rcc->APB1ENR &= ~f->gate;
        nx_arch_dsb();
        if ((config->rcc->APB1ENR & f->gate) != 0U)
            return NX_ERROR_IO;
        state->gates_added &= ~UINT32_C(0x80000000);
    }
    config->rcc->APB2ENR &= ~state->gates_added;
    nx_arch_dsb();
    if ((config->rcc->APB2ENR & state->gates_added) != 0U)
        return NX_ERROR_IO;
    state->gates_added = 0U;
    state->effects = 0U;
    state->initialized = false;
    state->closing = false;
    state->config = NULL;
    return NX_SUCCESS;
}
/** \brief One optional table shared by every statically selected group. */
const nx_adc_group_ops_t nx_stm32_adc_group_ops = {
    .submit = submit,
    .cancel = cancel,
    .service = service,
    .stop = stop,
    .info = info,
};
