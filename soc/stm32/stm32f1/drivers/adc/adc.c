/**
 * \file            adc.c
 * \brief           STM32F103 fixed-route regular sampling and calibration.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/adc/instance.h"
#include "drivers/adc/regular_calibration.h"
#include "drivers/adc/regular_poll.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_adc.h"
#include "system/platform.h"

#if defined(STM32F103xB)
#define F1_ADC       ADC1
#define F1_ADC_GATE  RCC_APB2ENR_ADC1EN
#define F1_ADC_RESET RCC_APB2RSTR_ADC1RST
#define F1_ADC_INPUT 0U
#define F1_ADC_IRQ   ADC1_2_IRQn
#define ADC_GATES    (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN)
#elif defined(STM32F103xE)
#define ADC_GATES (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN)
#if defined(NX_STM32F1_ADC3_POLL)
#define F1_ADC       ADC3
#define F1_ADC_GATE  RCC_APB2ENR_ADC3EN
#define F1_ADC_RESET RCC_APB2RSTR_ADC3RST
#define F1_ADC_INPUT 3U
#define F1_ADC_IRQ   ADC3_IRQn
#else
#define F1_ADC       ADC2
#define F1_ADC_GATE  RCC_APB2ENR_ADC2EN
#define F1_ADC_RESET RCC_APB2RSTR_ADC2RST
#define F1_ADC_INPUT 2U
#define F1_ADC_IRQ   ADC1_2_IRQn
#endif
#else
#error "Regular polling requires a reviewed STM32F103 ADC route"
#endif

#define SAMPLE_SHIFT (3U * F1_ADC_INPUT)
#define SAMPLE_MASK  (ADC_SMPR2_SMP0 << SAMPLE_SHIFT)

#define ADC_READY_CONFIG (LL_ADC_REG_TRIG_SOFTWARE | ADC_CR2_ADON)
#define ADC_CLOCK_HZ     4000000U
#define APB2_CLOCK_HZ    8000000U

enum {
    ADC_GATE = 1U,
    ADC_REGISTERS = 2U,
    ADC_LIVE = 4U,
    ADC_ACTIVE = 8U,
    ADC_FAULTED = 16U
};

static bool gate_owned(void) {
    return (READ_REG(RCC->APB2ENR) & F1_ADC_GATE) != 0U;
}

static nx_result_t prepare_regular(nx_stm32f1_adc_t* state, uint32_t apb2_hz,
                                   uint32_t reference_mv, uint32_t sample_code,
                                   uint32_t second_code) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || reference_mv < 2400U || reference_mv > 3600U ||
        sample_code >= sizeof(nx_stm32f1_adc_regular_ticks)) {
        return NX_ERROR_INVALID;
    }
#if defined(NX_STM32F1_ADC_SCAN_DMA)
    if (second_code >= sizeof(nx_stm32f1_adc_regular_ticks))
        return NX_ERROR_INVALID;
    const uint32_t scan_control = ADC_CR1_SCAN;
    const uint32_t rank_count = ADC_SQR1_L_0;
    const uint32_t ranks = 1U << ADC_SQR3_SQ2_Pos;
    const uint32_t pins = 3U;
    const uint32_t sample_mask = ADC_SMPR2_SMP0 | ADC_SMPR2_SMP1;
    const uint32_t sample_time =
        sample_code | (second_code << ADC_SMPR2_SMP1_Pos);
#else
    (void)second_code;
    const uint32_t scan_control = 0U;
    const uint32_t rank_count = 0U;
    const uint32_t ranks = F1_ADC_INPUT;
    const uint32_t pins = 1U << F1_ADC_INPUT;
    const uint32_t sample_mask = SAMPLE_MASK;
    const uint32_t sample_time = sample_code << SAMPLE_SHIFT;
#endif
    if (apb2_hz != APB2_CLOCK_HZ || !nx_stm32f1_system_live() ||
        !nx_stm32f1_adc_hsi8_clock()) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->effects != 0U || state->pads.registers != NULL ||
        (READ_REG(RCC->APB2ENR) & ADC_GATES) != 0U ||
        (READ_REG(RCC->APB2RSTR) & F1_ADC_RESET) != 0U ||
        !NX_STM32F1_ADC_IRQ_IDLE(F1_ADC_IRQ)) {
        return NX_ERROR_BUSY;
    }
    state->effects |= ADC_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB2ENR, F1_ADC_GATE, F1_ADC_GATE)) {
        return NX_ERROR_IO;
    }
    state->effects |= ADC_REGISTERS;
    if (!nx_stm32f1_adc_reset(F1_ADC, F1_ADC_RESET)) {
        return NX_ERROR_IO;
    }
    nx_result_t result = nx_stm32f1_pads_capture(&state->pads, GPIOA, pins);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_stm32f1_pads_apply(&state->pads, 0U, 0U, state->pads.latch);
    if (result != NX_SUCCESS) {
        return result;
    }

    /* ADC is reset and independent; only the selected regular ranks are armed.
     */
    MODIFY_REG(F1_ADC->SMPR2, sample_mask, sample_time);
#if defined(NX_STM32F1_ADC_SCAN_DMA)
    WRITE_REG(F1_ADC->CR1, scan_control);
    WRITE_REG(F1_ADC->SQR1, rank_count);
#endif
    WRITE_REG(F1_ADC->SQR3, ranks);
    LL_ADC_REG_SetTriggerSource(F1_ADC, LL_ADC_REG_TRIG_SOFTWARE);
    nx_arch_dsb();
    if (READ_REG(F1_ADC->CR1) != scan_control ||
        READ_REG(F1_ADC->CR2) != LL_ADC_REG_TRIG_SOFTWARE ||
        (READ_REG(F1_ADC->SMPR2) & sample_mask) != sample_time ||
        READ_REG(F1_ADC->SQR1) != rank_count ||
        READ_REG(F1_ADC->SQR3) != ranks) {
        return NX_ERROR_IO;
    }
    result = nx_stm32f1_adc_calibrate(F1_ADC, ADC_READY_CONFIG);
    if (result != NX_SUCCESS) {
        return nx_stm32f1_adc_power_down(F1_ADC) ? result : NX_ERROR_IO;
    }
    state->reference_mv = (uint16_t)reference_mv;
    state->conversion_ticks = nx_stm32f1_adc_regular_ticks[sample_code];
    state->effects |= ADC_LIVE;
    return NX_SUCCESS;
}

#if defined(NX_STM32F1_ADC_SCAN_DMA)
nx_result_t nx_stm32f1_adc_prepare_scan(nx_stm32f1_adc_t* state,
                                        uint32_t apb2_hz, uint32_t reference_mv,
                                        uint32_t first, uint32_t second,
                                        uint16_t* scan_ticks) {
    const nx_result_t result =
        prepare_regular(state, apb2_hz, reference_mv, first, second);
    if (result == NX_SUCCESS)
        *scan_ticks = (uint16_t)(state->conversion_ticks +
                                 nx_stm32f1_adc_regular_ticks[second]);
    return result;
}
#else
nx_result_t nx_stm32f1_adc_prepare(nx_stm32f1_adc_t* state, uint32_t apb2_hz,
                                   uint32_t reference_mv,
                                   uint32_t sample_code) {
    return prepare_regular(state, apb2_hz, reference_mv, sample_code, 0U);
}
#endif

nx_result_t nx_stm32f1_adc_close(nx_stm32f1_adc_t* state) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if ((state->effects & ADC_ACTIVE) != 0U) {
        return NX_ERROR_BUSY;
    }
    state->effects &= (uint8_t)~ADC_LIVE;
    if ((state->effects & ADC_REGISTERS) != 0U) {
        if (!gate_owned() || !nx_stm32f1_adc_power_down(F1_ADC) ||
            !nx_stm32f1_adc_reset(F1_ADC, F1_ADC_RESET)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~ADC_REGISTERS;
    }
    const nx_result_t pads = nx_stm32f1_pads_restore(&state->pads);
    if (pads != NX_SUCCESS) {
        return pads;
    }
    if ((state->effects & ADC_GATE) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, F1_ADC_GATE, 0U)) {
        return NX_ERROR_IO;
    }
    state->effects = 0U;
    return NX_SUCCESS;
}

static bool output_span(const nx_stm32f1_adc_t* state, const void* output,
                        size_t bytes, size_t alignment,
                        const nx_adc_ops_t* ops) {
    return nx_io_dispatch_span(output, bytes, alignment) &&
           !nx_io_dispatch_overlap(output, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(output, bytes, F1_ADC, sizeof(*F1_ADC)) &&
           !nx_io_dispatch_overlap(output, bytes, GPIOA, sizeof(*GPIOA)) &&
           !nx_io_dispatch_overlap(output, bytes, RCC, sizeof(*RCC)) &&
           !nx_io_dispatch_overlap(output, bytes, ops, sizeof(*ops));
}

nx_result_t nx_stm32f1_adc_info(const void* context, nx_adc_info_t* output,
                                const nx_adc_ops_t* ops) {
    const nx_stm32f1_adc_t* state = context;
    if (state == NULL || (state->effects & ADC_LIVE) == 0U ||
        !output_span(state, output, sizeof(*output), _Alignof(nx_adc_info_t),
                     ops)) {
        return NX_ERROR_INVALID;
    }
    *output = (nx_adc_info_t){12U, state->reference_mv, 1U};
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_adc_sequence_info(const void* context,
                                         nx_adc_sequence_info_t* output,
                                         const nx_adc_ops_t* ops) {
    const nx_stm32f1_adc_t* state = context;
    if (state == NULL || !output_span(state, output, sizeof(*output),
                                      _Alignof(nx_adc_sequence_info_t), ops)) {
        return NX_ERROR_INVALID;
    }
    if ((state->effects & ADC_LIVE) == 0U) {
        return NX_ERROR_STATE;
    }
    *output =
        (nx_adc_sequence_info_t){1U, state->conversion_ticks, ADC_CLOCK_HZ};
    return NX_SUCCESS;
}

#if !defined(NX_STM32F1_ADC_SCAN_DMA)
static nx_result_t fail_sample(nx_stm32f1_adc_t* state, nx_result_t result) {
    state->effects |= ADC_FAULTED;
    const bool stopped = gate_owned() && nx_stm32f1_adc_power_down(F1_ADC);
    state->effects &= (uint8_t)~ADC_ACTIVE;
    return stopped ? result : NX_ERROR_IO;
}

static nx_result_t sample(void* context, uint16_t* samples, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_stm32f1_adc_t* state = context;
    if (state == NULL || capacity == 0U ||
        capacity > SIZE_MAX / sizeof(*samples) ||
        !output_span(state, samples, capacity * sizeof(*samples),
                     _Alignof(uint16_t), &nx_stm32f1_adc_ops) ||
        !output_span(state, count, sizeof(*count), _Alignof(size_t),
                     &nx_stm32f1_adc_ops) ||
        nx_io_dispatch_overlap(samples, capacity * sizeof(*samples), count,
                               sizeof(*count))) {
        return NX_ERROR_INVALID;
    }
    *count = 0U;
    if ((state->effects & ADC_LIVE) == 0U) {
        return NX_ERROR_STATE;
    }
    if ((state->effects & (ADC_ACTIVE | ADC_FAULTED)) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (!nx_stm32f1_adc_hsi8_clock() ||
        (READ_REG(RCC->APB2ENR) & ADC_GATES) != F1_ADC_GATE ||
        READ_REG(F1_ADC->CR1) != 0U ||
        READ_REG(F1_ADC->CR2) != ADC_READY_CONFIG) {
        return fail_sample(state, NX_ERROR_IO);
    }
    WRITE_REG(F1_ADC->SR, 0U);
    nx_arch_dsb();
    if (READ_REG(F1_ADC->SR) != 0U) {
        return fail_sample(state, NX_ERROR_IO);
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    state->effects |= ADC_ACTIVE;
    uint16_t value;
    const nx_result_t result =
        nx_stm32f1_adc_poll_rank(F1_ADC, deadline, &value);
    if (result != NX_SUCCESS)
        return fail_sample(state, result);
    *samples = value;
    *count = 1U;
    /* A redundant CR2 write with ADON=1 may start a second conversion. */
    if ((READ_REG(F1_ADC->CR2) & ADC_CR2_EXTTRIG) != 0U) {
        LL_ADC_REG_StopConversionExtTrig(F1_ADC);
        nx_arch_dsb();
    }
    if (READ_REG(F1_ADC->CR2) != ADC_READY_CONFIG ||
        (READ_REG(F1_ADC->SR) & ADC_SR_EOC) != 0U)
        return fail_sample(state, NX_ERROR_IO);
    state->effects &= (uint8_t)~ADC_ACTIVE;
    return NX_SUCCESS;
}

static nx_result_t info(const void* context, nx_adc_info_t* out) {
    return nx_stm32f1_adc_info(context, out, &nx_stm32f1_adc_ops);
}

static nx_result_t sequence_info(const void* context,
                                 nx_adc_sequence_info_t* out) {
    return nx_stm32f1_adc_sequence_info(context, out, &nx_stm32f1_adc_ops);
}

const nx_adc_ops_t nx_stm32f1_adc_ops = {
    .info = info,
    .sample = sample,
    .sequence_info = sequence_info,
};
#endif
