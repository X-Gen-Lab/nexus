/**
 * \file            adc.c
 * \brief           STM32F103 regular PA0 sampling and bounded calibration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/adc/instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_adc.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "ADC1 regular single-shot requires the reviewed STM32F103xB"
#endif

#ifndef NX_STM32F1_ADC_IRQ_IDLE
#define NX_STM32F1_ADC_IRQ_IDLE()                                              \
    (NVIC_GetEnableIRQ(ADC1_2_IRQn) == 0U &&                                   \
     NVIC_GetPendingIRQ(ADC1_2_IRQn) == 0U &&                                  \
     NVIC_GetActive(ADC1_2_IRQn) == 0U)
#endif

#define ADC_READY_CONFIG (LL_ADC_REG_TRIG_SOFTWARE | ADC_CR2_ADON)
#define ADC_CLOCK_HZ     4000000U
#define APB2_CLOCK_HZ    8000000U

enum {
    ADC_GATE = 1U,
    ADC_REGISTERS = 2U,
    ADC_LIVE = 4U,
    ADC_ACTIVE = 8U,
    ADC_FAULTED = 16U,
    POWER_SETTLE_US = 4U,
    CALIBRATION_BUDGET_US = 256U,
    POLL_LIMIT = 4096U
};

/* RM0008 sampling windows plus DS5319 Table 47's 12.5 SAR cycles. */
static const uint8_t conversion_ticks[] = {14U, 20U, 26U, 41U,
                                           54U, 68U, 84U, 252U};

static bool clock_ready(void) {
    const uint32_t fields =
        RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE;
    return (READ_REG(RCC->CFGR) & fields) == RCC_CFGR_SWS_HSI;
}

static bool reset_owned_adc(void) {
    const uint32_t reset = RCC_APB2RSTR_ADC1RST;
    const bool asserted = nx_stm32f1_write_masked(&RCC->APB2RSTR, reset, reset);
    return nx_stm32f1_write_masked(&RCC->APB2RSTR, reset, 0U) && asserted &&
           READ_REG(ADC1->CR1) == 0U && READ_REG(ADC1->CR2) == 0U &&
           READ_REG(ADC1->SR) == 0U;
}

static bool power_down(void) {
    /* ADON=0 aborts conversion and calibration without any output loan. */
    WRITE_REG(ADC1->CR2, 0U);
    nx_arch_dsb();
    return READ_REG(ADC1->CR2) == 0U;
}

static bool power_settled(void) {
    const nx_time_us_t start = nx_time_now_us();
    for (unsigned i = 0U; i < POLL_LIMIT; ++i) {
        if (nx_time_now_us() - start >= POWER_SETTLE_US) {
            return true;
        }
    }
    return false;
}

static bool calibration_finished(uint32_t flag, nx_time_us_t deadline) {
    for (unsigned i = 0U; i < POLL_LIMIT; ++i) {
        if ((READ_REG(ADC1->CR2) & flag) == 0U) {
            return !nx_deadline_expired(deadline, nx_time_now_us());
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            break;
        }
    }
    return false;
}

static nx_result_t calibrate(void) {
    nx_time_us_t deadline;
    nx_result_t result = nx_deadline_finite_after(
        nx_time_now_us(), CALIBRATION_BUDGET_US, &deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    LL_ADC_Enable(ADC1);
    nx_arch_dsb();
    if (READ_REG(ADC1->CR2) != ADC_READY_CONFIG) {
        return NX_ERROR_IO;
    }
    /* Four nominal microseconds cover tSTAB and two ADC cycles at 4 MHz. */
    if (!power_settled() || nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    SET_BIT(ADC1->CR2, ADC_CR2_RSTCAL);
    nx_arch_dsb();
    if (!calibration_finished(ADC_CR2_RSTCAL, deadline)) {
        return NX_ERROR_TIMEOUT;
    }
    LL_ADC_StartCalibration(ADC1);
    nx_arch_dsb();
    if (!calibration_finished(ADC_CR2_CAL, deadline)) {
        return NX_ERROR_TIMEOUT;
    }
    WRITE_REG(ADC1->SR, 0U);
    nx_arch_dsb();
    return READ_REG(ADC1->CR2) == ADC_READY_CONFIG && READ_REG(ADC1->SR) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

nx_result_t nx_stm32f1_adc_prepare(nx_stm32f1_adc_t* state, uint32_t apb2_hz,
                                   uint32_t reference_mv,
                                   uint32_t sample_code) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || reference_mv < 2400U || reference_mv > 3600U ||
        sample_code >= sizeof(conversion_ticks)) {
        return NX_ERROR_INVALID;
    }
    if (apb2_hz != APB2_CLOCK_HZ || !nx_stm32f1_system_live() ||
        !clock_ready()) {
        return NX_ERROR_UNSUPPORTED;
    }
    const uint32_t gates = RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN;
    if (state->effects != 0U || state->pads.registers != NULL ||
        (READ_REG(RCC->APB2ENR) & gates) != 0U ||
        (READ_REG(RCC->APB2RSTR) & RCC_APB2RSTR_ADC1RST) != 0U ||
        !NX_STM32F1_ADC_IRQ_IDLE()) {
        return NX_ERROR_BUSY;
    }
    state->effects |= ADC_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_ADC1EN,
                                 RCC_APB2ENR_ADC1EN)) {
        return NX_ERROR_IO;
    }
    state->effects |= ADC_REGISTERS;
    if (!reset_owned_adc()) {
        return NX_ERROR_IO;
    }
    nx_result_t result = nx_stm32f1_pads_capture(&state->pads, GPIOA, 1U);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_stm32f1_pads_apply(&state->pads, 0U, 0U, state->pads.latch);
    if (result != NX_SUCCESS) {
        return result;
    }

    /* This reset ADC has a single regular rank on channel 0. Avoid injected
     * triggers entirely, including the PA0 coupling described by ES096. */
    MODIFY_REG(ADC1->SMPR2, ADC_SMPR2_SMP0, sample_code);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    nx_arch_dsb();
    if (READ_REG(ADC1->CR1) != 0U ||
        READ_REG(ADC1->CR2) != LL_ADC_REG_TRIG_SOFTWARE ||
        (READ_REG(ADC1->SMPR2) & ADC_SMPR2_SMP0) != sample_code ||
        READ_REG(ADC1->SQR1) != 0U || READ_REG(ADC1->SQR3) != 0U) {
        return NX_ERROR_IO;
    }
    result = calibrate();
    if (result != NX_SUCCESS) {
        return power_down() ? result : NX_ERROR_IO;
    }
    state->reference_mv = (uint16_t)reference_mv;
    state->conversion_ticks = conversion_ticks[sample_code];
    state->effects |= ADC_LIVE;
    return NX_SUCCESS;
}

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
        if (!power_down() || !reset_owned_adc()) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~ADC_REGISTERS;
    }
    const nx_result_t pads = nx_stm32f1_pads_restore(&state->pads);
    if (pads != NX_SUCCESS) {
        return pads;
    }
    if ((state->effects & ADC_GATE) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_ADC1EN, 0U)) {
        return NX_ERROR_IO;
    }
    state->effects = 0U;
    return NX_SUCCESS;
}

static bool output_span(const nx_stm32f1_adc_t* state, const void* output,
                        size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(output, bytes, alignment) &&
           !nx_io_dispatch_overlap(output, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(output, bytes, ADC1, sizeof(*ADC1)) &&
           !nx_io_dispatch_overlap(output, bytes, GPIOA, sizeof(*GPIOA)) &&
           !nx_io_dispatch_overlap(output, bytes, RCC, sizeof(*RCC)) &&
           !nx_io_dispatch_overlap(output, bytes, &nx_stm32f1_adc_ops,
                                   sizeof(nx_stm32f1_adc_ops));
}

static nx_result_t info(const void* context, nx_adc_info_t* output) {
    const nx_stm32f1_adc_t* state = context;
    if (state == NULL || (state->effects & ADC_LIVE) == 0U ||
        !output_span(state, output, sizeof(*output), _Alignof(nx_adc_info_t))) {
        return NX_ERROR_INVALID;
    }
    *output = (nx_adc_info_t){12U, state->reference_mv, 1U};
    return NX_SUCCESS;
}

static nx_result_t sequence_info(const void* context,
                                 nx_adc_sequence_info_t* output) {
    const nx_stm32f1_adc_t* state = context;
    if (state == NULL || !output_span(state, output, sizeof(*output),
                                      _Alignof(nx_adc_sequence_info_t))) {
        return NX_ERROR_INVALID;
    }
    if ((state->effects & ADC_LIVE) == 0U) {
        return NX_ERROR_STATE;
    }
    *output =
        (nx_adc_sequence_info_t){1U, state->conversion_ticks, ADC_CLOCK_HZ};
    return NX_SUCCESS;
}

static nx_result_t fail_sample(nx_stm32f1_adc_t* state, nx_result_t result) {
    state->effects |= ADC_FAULTED;
    const bool stopped = power_down();
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
                     _Alignof(uint16_t)) ||
        !output_span(state, count, sizeof(*count), _Alignof(size_t)) ||
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
    if (!clock_ready() || READ_REG(ADC1->CR2) != ADC_READY_CONFIG) {
        return fail_sample(state, NX_ERROR_IO);
    }
    WRITE_REG(ADC1->SR, 0U);
    nx_arch_dsb();
    if (READ_REG(ADC1->SR) != 0U) {
        return fail_sample(state, NX_ERROR_IO);
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    state->effects |= ADC_ACTIVE;
    LL_ADC_REG_StartConversionSWStart(ADC1);
    nx_arch_dsb();
    for (unsigned i = 0U; i < POLL_LIMIT; ++i) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            break;
        }
        if ((READ_REG(ADC1->SR) & (ADC_SR_EOC | ADC_SR_STRT)) !=
            (ADC_SR_EOC | ADC_SR_STRT)) {
            continue;
        }
        const uint16_t value = LL_ADC_REG_ReadConversionData12(ADC1);
        if (value > 0x0fffU) {
            return fail_sample(state, NX_ERROR_IO);
        }
        *samples = value;
        *count = 1U;
        /* A redundant CR2 write with ADON=1 may start a second conversion. */
        if ((READ_REG(ADC1->CR2) & ADC_CR2_EXTTRIG) != 0U) {
            LL_ADC_REG_StopConversionExtTrig(ADC1);
            nx_arch_dsb();
        }
        if (READ_REG(ADC1->CR2) != ADC_READY_CONFIG ||
            (READ_REG(ADC1->SR) & ADC_SR_EOC) != 0U) {
            return fail_sample(state, NX_ERROR_IO);
        }
        state->effects &= (uint8_t)~ADC_ACTIVE;
        return NX_SUCCESS;
    }
    return fail_sample(state, NX_ERROR_TIMEOUT);
}

const nx_adc_ops_t nx_stm32f1_adc_ops = {
    .info = info,
    .sample = sample,
    .sequence_info = sequence_info,
};
