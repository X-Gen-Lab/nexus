/**
 * \file            internal.c
 * \brief           Bounded ADC1 temperature-sensor and VREFINT raw pair
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "internal.h"
#include "dispatch_checks.h"
#include "drivers/adc/regular_calibration.h"
#include "drivers/adc/regular_poll.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "Internal ADC pair requires the reviewed STM32F103xB"
#endif

#define ADC_GATE    RCC_APB2ENR_ADC1EN
#define ADC_SHARED  (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN)
#define READY       (LL_ADC_REG_TRIG_SOFTWARE | ADC_CR2_ADON)
#define SENSOR      (READY | ADC_CR2_TSVREFE)
#define SAMPLE_CODE 6U
#define TIMING                                                                 \
    ((SAMPLE_CODE << ADC_SMPR1_SMP16_Pos) |                                    \
     (SAMPLE_CODE << ADC_SMPR1_SMP17_Pos))

enum {
    GATE_OWNED = 1U,
    REGS_OWNED = 2U,
    LIVE = 4U,
    ACTIVE = 8U,
    FAULT = 16U,
    STARTUP_US = 12U,
    RAW_COUNT = 2U
};

static bool clock_ready(void) {
    const uint32_t hsi = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSITRIM;
    const uint32_t reset_trim = UINT32_C(16) << RCC_CR_HSITRIM_Pos;
    return nx_stm32f1_system_live() && nx_stm32f1_adc_hsi8_clock() &&
           (RCC->CR & hsi) == (RCC_CR_HSION | RCC_CR_HSIRDY | reset_trim);
}

static bool gate_live(void) {
    return (RCC->APB2ENR & ADC_SHARED) == ADC_GATE;
}

static bool configured(void) {
    return READ_REG(ADC1->CR1) == 0U && READ_REG(ADC1->SQR1) == 0U &&
           READ_REG(ADC1->SMPR1) == TIMING;
}

nx_result_t nx_stm32f1_adc_internal_prepare(nx_stm32f1_adc_internal_t* state,
                                            uint32_t apb2_hz,
                                            uint32_t reference_mv) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || reference_mv < 2400U || reference_mv > 3600U)
        return NX_ERROR_INVALID;
    if (apb2_hz != 8000000U || !clock_ready())
        return NX_ERROR_UNSUPPORTED;
    if (state->effects != 0U || (RCC->APB2ENR & ADC_SHARED) != 0U ||
        (RCC->APB2RSTR & RCC_APB2RSTR_ADC1RST) != 0U ||
        !NX_STM32F1_ADC_IRQ_IDLE(ADC1_2_IRQn))
        return NX_ERROR_BUSY;

    state->effects |= GATE_OWNED;
    if (!nx_stm32f1_write_masked(&RCC->APB2ENR, ADC_GATE, ADC_GATE))
        return NX_ERROR_IO;

    /* Reading requires its clock. A previously gated ADC may still have
     * an inherited internal path; leave that register unchanged. */
    if ((READ_REG(ADC1->CR2) & (ADC_CR2_ADON | ADC_CR2_TSVREFE)) != 0U)
        return NX_ERROR_BUSY;
    state->effects |= REGS_OWNED;
    if (!nx_stm32f1_adc_reset(ADC1, RCC_APB2RSTR_ADC1RST))
        return NX_ERROR_IO;
    WRITE_REG(ADC1->SMPR1, TIMING);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    nx_arch_dsb();
    if (!configured())
        return NX_ERROR_IO;
    const nx_result_t result = nx_stm32f1_adc_calibrate(ADC1, READY);
    if (result != NX_SUCCESS)
        return nx_stm32f1_adc_power_down(ADC1) ? result : NX_ERROR_IO;
    state->reference_mv = (uint16_t)reference_mv;
    state->effects |= LIVE;
    return NX_SUCCESS;
}

static bool powered_off(void) {
    return gate_live() && nx_stm32f1_adc_power_down(ADC1);
}

nx_result_t nx_stm32f1_adc_internal_close(nx_stm32f1_adc_internal_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if ((state->effects & ACTIVE) != 0U)
        return NX_ERROR_BUSY;
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & REGS_OWNED) != 0U) {
        if (!powered_off() || !nx_stm32f1_adc_reset(ADC1, RCC_APB2RSTR_ADC1RST))
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~REGS_OWNED;
    }
    if ((state->effects & GATE_OWNED) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, ADC_GATE, 0U))
        return NX_ERROR_IO;
    state->effects = 0U;
    return NX_SUCCESS;
}

static bool independent(const nx_stm32f1_adc_internal_t* state, const void* p,
                        size_t bytes, size_t align) {
    return nx_io_dispatch_span(p, bytes, align) &&
           !nx_io_dispatch_overlap(p, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(p, bytes, (void*)PERIPH_BASE,
                                   UINT32_C(0x20000000)) &&
           !nx_io_dispatch_overlap(p, bytes, ADC1, sizeof(*ADC1)) &&
           !nx_io_dispatch_overlap(p, bytes, RCC, sizeof(*RCC)) &&
           !nx_io_dispatch_overlap(p, bytes, &nx_stm32f1_adc_internal_ops,
                                   sizeof(nx_stm32f1_adc_internal_ops));
}

static bool warmup(nx_time_us_t deadline) {
    const nx_time_us_t enabled = nx_time_now_us();
    for (unsigned poll = 0U; poll < ADC_REGULAR_POLLS; ++poll) {
        const nx_time_us_t now = nx_time_now_us();
        if (nx_deadline_expired(deadline, now))
            return false;
        if (now - enabled >= STARTUP_US)
            return true;
    }
    return false;
}

static nx_result_t sample_rank(uint32_t channel, nx_time_us_t deadline,
                               uint16_t* raw) {
    WRITE_REG(ADC1->SQR3, channel);
    WRITE_REG(ADC1->SR, 0U);
    nx_arch_dsb();
    if (READ_REG(ADC1->SQR3) != channel || READ_REG(ADC1->SR) != 0U ||
        READ_REG(ADC1->CR2) != SENSOR)
        return NX_ERROR_IO;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    const nx_result_t result = nx_stm32f1_adc_poll_rank(ADC1, deadline, raw);
    if (result != NX_SUCCESS)
        return result;
    if ((READ_REG(ADC1->CR2) & ADC_CR2_EXTTRIG) != 0U) {
        LL_ADC_REG_StopConversionExtTrig(ADC1);
        nx_arch_dsb();
    }
    return READ_REG(ADC1->CR2) == SENSOR &&
                   (READ_REG(ADC1->SR) & ADC_SR_EOC) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

static nx_result_t sample(void* context, uint16_t* values, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    nx_stm32f1_adc_internal_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || capacity < RAW_COUNT ||
        capacity > SIZE_MAX / sizeof(*values) ||
        !independent(state, values, capacity * sizeof(*values),
                     _Alignof(uint16_t)) ||
        !independent(state, count, sizeof(*count), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(values, capacity * sizeof(*values), count,
                               sizeof(*count)))
        return NX_ERROR_INVALID;
    *count = 0U;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    if ((state->effects & (ACTIVE | FAULT)) != 0U)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (!clock_ready() || !gate_live() || !configured() ||
        !NX_STM32F1_ADC_IRQ_IDLE(ADC1_2_IRQn) || READ_REG(ADC1->CR2) != READY)
        return NX_ERROR_IO;

    state->effects |= ACTIVE;
    WRITE_REG(ADC1->CR2, SENSOR);
    nx_arch_dsb();
    uint16_t raw[RAW_COUNT];
    nx_result_t result = READ_REG(ADC1->CR2) != SENSOR ? NX_ERROR_IO
                         : !warmup(deadline)           ? NX_ERROR_TIMEOUT
                                             : sample_rank(16U, deadline, raw);
    if (result == NX_SUCCESS)
        result = sample_rank(17U, deadline, raw + 1);
    if (result == NX_SUCCESS) {
        /* ADON remains set while the changed TSVREFE bit turns the path off. */
        WRITE_REG(ADC1->CR2, READY);
        nx_arch_dsb();
        if (READ_REG(ADC1->CR2) != READY || !gate_live() || !clock_ready())
            result = NX_ERROR_IO;
        else if (nx_deadline_expired(deadline, nx_time_now_us()))
            result = NX_ERROR_TIMEOUT;
    }
    state->effects &= (uint8_t)~ACTIVE;
    if (result != NX_SUCCESS) {
        state->effects |= FAULT;
        return powered_off() ? result : NX_ERROR_IO;
    }
    values[0] = raw[0];
    values[1] = raw[1];
    *count = RAW_COUNT;
    return NX_SUCCESS;
}

static nx_result_t info(const void* context, nx_adc_info_t* out) {
    const nx_stm32f1_adc_internal_t* state = context;
    if (state == NULL ||
        !independent(state, out, sizeof(*out), _Alignof(nx_adc_info_t)))
        return NX_ERROR_INVALID;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    *out = (nx_adc_info_t){.resolution_bits = 12U,
                           .reference_mv = state->reference_mv,
                           .channel_count = RAW_COUNT};
    return NX_SUCCESS;
}

static nx_result_t timing(const void* context, nx_adc_sequence_info_t* out) {
    const nx_stm32f1_adc_internal_t* state = context;
    if (state == NULL || !independent(state, out, sizeof(*out),
                                      _Alignof(nx_adc_sequence_info_t)))
        return NX_ERROR_INVALID;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    *out = (nx_adc_sequence_info_t){
        .channel_count = RAW_COUNT,
        .scan_ticks = RAW_COUNT * nx_stm32f1_adc_regular_ticks[SAMPLE_CODE],
        .ticks_per_second = 4000000U};
    return NX_SUCCESS;
}

const nx_adc_ops_t nx_stm32f1_adc_internal_ops = {
    .info = info, .sample = sample, .sequence_info = timing};
