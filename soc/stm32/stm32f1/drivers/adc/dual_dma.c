/**
 * \file            dual_dma.c
 * \brief           F103ZE regular simultaneous ADC with one packed DMA word
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/adc/dual_dma.h"
#include "dispatch_checks.h"
#include "drivers/adc/regular_calibration.h"
#include "system/platform.h"

#if !defined(STM32F103xE)
#error "Regular simultaneous ADC requires the reviewed STM32F103xE"
#endif

#ifndef NX_STM32F1_ADC_DUAL_IRQS_IDLE
#define NX_STM32F1_ADC_DUAL_IRQS_IDLE()                                        \
    (NVIC_GetEnableIRQ(ADC1_2_IRQn) == 0U &&                                   \
     NVIC_GetPendingIRQ(ADC1_2_IRQn) == 0U &&                                  \
     NVIC_GetActive(ADC1_2_IRQn) == 0U &&                                      \
     NVIC_GetEnableIRQ(DMA1_Channel1_IRQn) == 0U &&                            \
     NVIC_GetPendingIRQ(DMA1_Channel1_IRQn) == 0U &&                           \
     NVIC_GetActive(DMA1_Channel1_IRQn) == 0U)
#endif

#define ADC_GATES    (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN)
#define ADC_RESETS   (RCC_APB2RSTR_ADC1RST | RCC_APB2RSTR_ADC2RST)
#define MASTER_READY (LL_ADC_REG_TRIG_SOFTWARE | ADC_CR2_ADON)
#define SLAVE_READY  (MASTER_READY | ADC_CR2_EXTTRIG)
#define MASTER_DMA   (SLAVE_READY | ADC_CR2_DMA)
#define DMA_EVENTS   (DMA_ISR_TCIF1 | DMA_ISR_HTIF1 | DMA_ISR_TEIF1)
#define DMA_ACK      (DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1)
#define DMA_WORDS    (DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1)

enum {
    DMA_GATE = 1U,
    ADC_GATE = 2U,
    CONVERTERS = 4U,
    LIVE = 8U,
    ARMED = 16U,
    ACTIVE = 32U,
    FAULTED = 64U,
    POLLS = 4096U
};

static bool clock_ready(void) {
    const uint32_t fields =
        RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE;
    return (READ_REG(RCC->CFGR) & fields) == RCC_CFGR_SWS_HSI;
}

static bool gates_live(void) {
    return (READ_REG(RCC->APB2ENR) & ADC_GATES) == ADC_GATES &&
           (READ_REG(RCC->AHBENR) & RCC_AHBENR_DMA1EN) != 0U;
}

static bool lane_idle(void) {
    return DMA1_Channel1->CCR == 0U && DMA1_Channel1->CNDTR == 0U &&
           DMA1_Channel1->CPAR == 0U && DMA1_Channel1->CMAR == 0U &&
           (DMA1->ISR & DMA_EVENTS) == 0U;
}

static bool reset_pair(void) {
    const bool asserted =
        nx_stm32f1_write_masked(&RCC->APB2RSTR, ADC_RESETS, ADC_RESETS);
    return nx_stm32f1_write_masked(&RCC->APB2RSTR, ADC_RESETS, 0U) &&
           asserted &&
           (ADC1->CR1 | ADC1->CR2 | ADC1->SR | ADC2->CR1 | ADC2->CR2 |
            ADC2->SR) == 0U;
}

static bool power_down_pair(void) {
    const bool master_off = nx_stm32f1_adc_power_down(ADC1);
    return nx_stm32f1_adc_power_down(ADC2) && master_off;
}

static bool pair_configured(uint32_t code) {
    return ADC1->CR1 == LL_ADC_MULTI_DUAL_REG_SIMULT && ADC2->CR1 == 0U &&
           ADC1->SQR1 == 0U && ADC1->SQR3 == 1U && ADC2->SQR1 == 0U &&
           ADC2->SQR3 == 2U && ADC1->SMPR2 == (code << ADC_SMPR2_SMP1_Pos) &&
           ADC2->SMPR2 == (code << ADC_SMPR2_SMP2_Pos);
}

nx_result_t nx_stm32f1_adc_dual_dma_prepare(
    nx_stm32f1_adc_dual_dma_t* state, uint32_t apb2_hz, uint32_t reference_mv,
    uint32_t sample_code, const nx_dma_memory_region_t* regions,
    size_t region_count) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || reference_mv < 2400U || reference_mv > 3600U ||
        region_count == 0U || region_count > 16U ||
        sample_code >= sizeof(nx_stm32f1_adc_regular_ticks))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        regions, region_count, (const void*)(uintptr_t)&state->packed,
        sizeof(state->packed), _Alignof(uint32_t), NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    if (apb2_hz != 8000000U || !nx_stm32f1_system_live() || !clock_ready())
        return NX_ERROR_UNSUPPORTED;
    if (state->effects != 0U || state->pads.registers != NULL ||
        (READ_REG(RCC->APB2ENR) & ADC_GATES) != 0U ||
        (READ_REG(RCC->APB2RSTR) & ADC_RESETS) != 0U ||
        !NX_STM32F1_ADC_DUAL_IRQS_IDLE())
        return NX_ERROR_BUSY;
    if ((READ_REG(RCC->AHBENR) & RCC_AHBENR_DMA1EN) == 0U) {
        state->effects |= DMA_GATE;
        if (!nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN,
                                     RCC_AHBENR_DMA1EN))
            return NX_ERROR_IO;
    }
    if (!lane_idle())
        return NX_ERROR_BUSY;
    result = nx_stm32f1_pads_capture(&state->pads, GPIOA, 6U);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32f1_pads_apply(&state->pads, 0U, 0U, state->pads.latch);
    if (result != NX_SUCCESS)
        return result;

    state->effects |= ADC_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB2ENR, ADC_GATES, ADC_GATES))
        return NX_ERROR_IO;
    state->effects |= CONVERTERS;
    if (!reset_pair())
        return NX_ERROR_IO;
    WRITE_REG(ADC1->SQR3, 1U);
    WRITE_REG(ADC2->SQR3, 2U);
    WRITE_REG(ADC1->SMPR2, sample_code << ADC_SMPR2_SMP1_Pos);
    WRITE_REG(ADC2->SMPR2, sample_code << ADC_SMPR2_SMP2_Pos);
    /* Both converters are disabled and their ranks are already fixed. */
    WRITE_REG(ADC1->CR1, LL_ADC_MULTI_DUAL_REG_SIMULT);
    WRITE_REG(ADC1->CR2, LL_ADC_REG_TRIG_SOFTWARE);
    WRITE_REG(ADC2->CR2, LL_ADC_REG_TRIG_SOFTWARE | ADC_CR2_EXTTRIG);
    result = nx_stm32f1_adc_calibrate(ADC1, MASTER_READY);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_adc_calibrate(ADC2, SLAVE_READY);
    if (result != NX_SUCCESS)
        return power_down_pair() ? result : NX_ERROR_IO;
    if (!pair_configured(sample_code) || ADC1->CR2 != MASTER_READY ||
        ADC2->CR2 != SLAVE_READY)
        return NX_ERROR_IO;
    state->reference_mv = (uint16_t)reference_mv;
    state->sample_code = (uint8_t)sample_code;
    state->effects |= LIVE;
    return NX_SUCCESS;
}

/** \brief Withdraw both producers first and then the owned DMA address. */
static nx_result_t drain(nx_stm32f1_adc_dual_dma_t* state, bool complete) {
    if (!gates_live())
        return NX_ERROR_IO;
    if (complete && ADC1->CR2 != MASTER_READY) {
        WRITE_REG(ADC1->CR2, MASTER_READY);
        nx_arch_dsb();
    }
    if (complete ? (ADC1->CR2 != MASTER_READY || ADC2->CR2 != SLAVE_READY)
                 : !power_down_pair())
        return NX_ERROR_IO;
    if (!nx_stm32f1_write_masked(&DMA1_Channel1->CCR, DMA_CCR_EN, 0U))
        return NX_ERROR_IO;

    const bool late_error = (DMA1->ISR & DMA_ISR_TEIF1) != 0U;
    /* ES0340 §2.4.1 requires the individual TC, HT and TE acknowledgments. */
    NX_STM32F1_WRITE(&DMA1->IFCR, DMA_ACK);
    NX_STM32F1_WRITE(&DMA1_Channel1->CCR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CNDTR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CPAR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CMAR, 0U);
    nx_arch_dsb();
    if (!lane_idle())
        return NX_ERROR_IO;
    state->effects &= (uint8_t)~ARMED;
    return complete && late_error ? NX_ERROR_IO : NX_SUCCESS;
}

nx_result_t nx_stm32f1_adc_dual_dma_close(nx_stm32f1_adc_dual_dma_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if ((state->effects & ACTIVE) != 0U)
        return NX_ERROR_BUSY;
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & ARMED) != 0U && drain(state, false) != NX_SUCCESS)
        return NX_ERROR_IO;
    if ((state->effects & CONVERTERS) != 0U) {
        if (!gates_live() || !power_down_pair() || !reset_pair())
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~CONVERTERS;
    }
    if ((state->effects & ADC_GATE) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, ADC_GATES, 0U))
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~ADC_GATE;
    }
    const nx_result_t pads = nx_stm32f1_pads_restore(&state->pads);
    if (pads != NX_SUCCESS)
        return pads;
    if ((state->effects & DMA_GATE) != 0U &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN, 0U))
        return NX_ERROR_IO;
    state->effects = 0U;
    return NX_SUCCESS;
}

static bool independent(const nx_stm32f1_adc_dual_dma_t* state, const void* ptr,
                        size_t bytes, size_t align) {
    return nx_io_dispatch_span(ptr, bytes, align) &&
           !nx_io_dispatch_overlap(ptr, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(ptr, bytes, (void*)PERIPH_BASE,
                                   UINT32_C(0x20000000)) &&
           !nx_io_dispatch_overlap(ptr, bytes, ADC1, sizeof(*ADC1)) &&
           !nx_io_dispatch_overlap(ptr, bytes, ADC2, sizeof(*ADC2)) &&
           !nx_io_dispatch_overlap(ptr, bytes, DMA1, sizeof(*DMA1)) &&
           !nx_io_dispatch_overlap(ptr, bytes, DMA1_Channel1,
                                   sizeof(*DMA1_Channel1)) &&
           !nx_io_dispatch_overlap(ptr, bytes, &nx_stm32f1_adc_dual_dma_ops,
                                   sizeof(nx_stm32f1_adc_dual_dma_ops));
}

static bool arm(nx_stm32f1_adc_dual_dma_t* state) {
    const uint32_t source = (uint32_t)(uintptr_t)&ADC1->DR;
    const uint32_t target = (uint32_t)(uintptr_t)&state->packed;
    WRITE_REG(ADC1->SR, 0U);
    WRITE_REG(ADC2->SR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CPAR, source);
    NX_STM32F1_WRITE(&DMA1_Channel1->CMAR, target);
    NX_STM32F1_WRITE(&DMA1_Channel1->CNDTR, 1U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CCR, DMA_WORDS);
    nx_arch_dsb();
    if ((ADC1->SR | ADC2->SR) != 0U || DMA1_Channel1->CPAR != source ||
        DMA1_Channel1->CMAR != target || DMA1_Channel1->CNDTR != 1U ||
        DMA1_Channel1->CCR != DMA_WORDS ||
        !nx_stm32f1_write_masked(&DMA1_Channel1->CCR, DMA_CCR_EN, DMA_CCR_EN))
        return false;
    WRITE_REG(ADC1->CR2, MASTER_DMA);
    nx_arch_dsb();
    if (ADC1->CR2 != MASTER_DMA)
        return false;
    LL_ADC_REG_StartConversionSWStart(ADC1);
    nx_arch_dsb();
    return true;
}

static nx_result_t completion(nx_time_us_t deadline) {
    for (unsigned poll = 0U; poll < POLLS; ++poll) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            break;
        const uint32_t flags = DMA1->ISR;
        if (!gates_live() || (flags & DMA_ISR_TEIF1) != 0U)
            return NX_ERROR_IO;
        if ((flags & DMA_ISR_TCIF1) != 0U)
            return DMA1_Channel1->CNDTR == 0U ? NX_SUCCESS : NX_ERROR_IO;
    }
    return NX_ERROR_TIMEOUT;
}

static nx_result_t sample(void* context, uint16_t* values, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    nx_stm32f1_adc_dual_dma_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || capacity < 2U ||
        capacity > SIZE_MAX / sizeof(*values) ||
        !independent(state, values, capacity * sizeof(*values),
                     _Alignof(uint16_t)) ||
        !independent(state, count, sizeof(*count), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(values, capacity * sizeof(*values), count,
                               sizeof(*count)))
        return NX_ERROR_INVALID;
    *count = 0U;
    if ((state->effects & (ACTIVE | ARMED)) != 0U)
        return NX_ERROR_BUSY;
    if ((state->effects & (LIVE | FAULTED)) != LIVE)
        return NX_ERROR_STATE;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (!gates_live() || !clock_ready() || !lane_idle() ||
        !NX_STM32F1_ADC_DUAL_IRQS_IDLE() ||
        !pair_configured(state->sample_code) || ADC1->CR2 != MASTER_READY ||
        ADC2->CR2 != SLAVE_READY)
        return NX_ERROR_IO;

    state->effects |= ARMED | ACTIVE;
    nx_result_t result = arm(state) ? completion(deadline) : NX_ERROR_IO;
    if (drain(state, result == NX_SUCCESS) != NX_SUCCESS)
        result = NX_ERROR_IO;
    state->effects &= (uint8_t)~ACTIVE;
    if (result == NX_SUCCESS) {
        const uint32_t packed = state->packed;
        if ((packed & UINT32_C(0xf000f000)) != 0U)
            result = NX_ERROR_IO;
        else if (nx_deadline_expired(deadline, nx_time_now_us()))
            result = NX_ERROR_TIMEOUT;
        else {
            values[0] = (uint16_t)packed;
            values[1] = (uint16_t)(packed >> 16U);
            *count = 2U;
            return NX_SUCCESS;
        }
    }
    state->effects |= FAULTED;
    return result;
}

static nx_result_t info(const void* context, nx_adc_info_t* out) {
    const nx_stm32f1_adc_dual_dma_t* state = context;
    if (state == NULL ||
        !independent(state, out, sizeof(*out), _Alignof(nx_adc_info_t)))
        return NX_ERROR_INVALID;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    *out = (nx_adc_info_t){.resolution_bits = 12U,
                           .channel_count = 2U,
                           .reference_mv = state->reference_mv};
    return NX_SUCCESS;
}

static nx_result_t sequence_info(const void* context,
                                 nx_adc_sequence_info_t* out) {
    const nx_stm32f1_adc_dual_dma_t* state = context;
    if (state == NULL || !independent(state, out, sizeof(*out),
                                      _Alignof(nx_adc_sequence_info_t)))
        return NX_ERROR_INVALID;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    *out = (nx_adc_sequence_info_t){
        .channel_count = 2U,
        .scan_ticks = nx_stm32f1_adc_regular_ticks[state->sample_code],
        .ticks_per_second = 4000000U};
    return NX_SUCCESS;
}

const nx_adc_ops_t nx_stm32f1_adc_dual_dma_ops = {
    .info = info, .sample = sample, .sequence_info = sequence_info};
