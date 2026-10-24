/**
 * \file            adc.c
 * \brief           Calibrated single-ended G474 ADC1 channel1 finite shots
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32g474_acquisition.h"
#include "stm32g4xx_ll_adc.h"
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32g474_adc_state_t* p) {
    return p != NULL && p->registers != NULL && p->peer != NULL &&
           p->registers != p->peer && p->common != NULL &&
           p->reference_mv >= 1620U && p->reference_mv <= 3600U &&
           p->poll_limit != 0U;
}
static nx_result_t wait_cr(nx_stm32g474_adc_state_t* p, uint32_t mask,
                           uint32_t expected) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_G474_ADC_POLL(p);
        if ((NX_G474_READ(&p->registers->CR) & mask) == expected)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_IO;
}
/** \brief A delay uses live independent time, with a second finite poll bound.
 */
static nx_result_t delay(nx_stm32g474_adc_state_t* p, uint32_t us) {
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), us);
    uint32_t remaining = p->poll_limit;
    do {
        NX_G474_ADC_POLL(p);
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
nx_result_t nx_stm32g474_adc_initialize(nx_stm32g474_adc_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->retained || p->initialized || p->active ||
        (NX_G474_READ(&p->registers->CR) & ~(uint32_t)ADC_CR_DEEPPWD) != 0U ||
        (NX_G474_READ(&p->peer->CR) & ~(uint32_t)ADC_CR_DEEPPWD) != 0U ||
        NX_G474_READ(&p->common->CCR) != 0U ||
        NX_G474_READ(&p->registers->IER) != 0U)
        return NX_ERROR_BUSY;
    if (!g_nx_stm32g474_system.started ||
        g_nx_stm32g474_system.core_hz != 170000000U ||
        g_nx_stm32g474_system.rcc == NULL ||
        (NX_G474_READ(&g_nx_stm32g474_system.rcc->CCIPR) &
         RCC_CCIPR_ADC12SEL) != RCC_CCIPR_ADC12SEL_1)
        return NX_ERROR_STATE;
    p->retained = true;
    p->fault = false;
    LL_ADC_SetCommonClock(p->common, LL_ADC_CLOCK_ASYNC_DIV128);
    nx_arch_dsb();
    if (NX_G474_READ(&p->common->CCR) != (10U << ADC_CCR_PRESC_Pos))
        return NX_ERROR_IO;
    NX_G474_WRITE(&p->registers->CR, ADC_CR_ADVREGEN);
    nx_result_t result = delay(p, 22U);
    if (result != NX_SUCCESS)
        return result;
    NX_G474_WRITE(&p->registers->DIFSEL, 0U);
    NX_G474_WRITE(&p->registers->CR, ADC_CR_ADVREGEN | ADC_CR_ADCAL);
    result = wait_cr(p, ADC_CR_ADCAL, 0U);
    if (result != NX_SUCCESS)
        return result;
    /* Five nominal1MHz ticks retain at least four complete ticks after
     * quantization:4*170/128=5.3125 ADCcycles exceeds the four-cycle minimum.
     */
    result = delay(p, 5U);
    if (result != NX_SUCCESS)
        return result;
    NX_G474_WRITE(&p->registers->CFGR, 0U);
    NX_G474_WRITE(&p->registers->CFGR2, ADC_CFGR2_BULB);
    NX_G474_WRITE(&p->registers->SQR1, 1U << ADC_SQR1_SQ1_Pos);
    NX_G474_WRITE(&p->registers->SMPR1, 0U);
    nx_arch_dsb();
    if (NX_G474_READ(&p->registers->CFGR) != 0U ||
        NX_G474_READ(&p->registers->CFGR2) != ADC_CFGR2_BULB ||
        NX_G474_READ(&p->registers->DIFSEL) != 0U ||
        NX_G474_READ(&p->registers->SQR1) != (1U << ADC_SQR1_SQ1_Pos) ||
        NX_G474_READ(&p->registers->SMPR1) != 0U)
        return NX_ERROR_IO;
    NX_G474_WRITE(&p->registers->ISR,
                  ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR);
    nx_arch_dsb();
    if ((NX_G474_READ(&p->registers->ISR) &
         (ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR)) != 0U)
        return NX_ERROR_IO;
    /* Leave ADEN clear: bulb sampling must not linger through caller idle. */
    p->initialized = true;
    return NX_SUCCESS;
}
/** \brief No caller buffer may leave until conversion stop is observed. */
static nx_result_t detach(nx_stm32g474_adc_state_t* p) {
    if ((NX_G474_READ(&p->registers->CR) & ADC_CR_ADSTART) != 0U) {
        NX_G474_WRITE(&p->registers->CR,
                      NX_G474_READ(&p->registers->CR) | ADC_CR_ADSTP);
        nx_result_t result = wait_cr(p, ADC_CR_ADSTART | ADC_CR_ADSTP, 0U);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_arch_dsb();
    return NX_SUCCESS;
}
nx_result_t nx_stm32g474_adc_quiesce(nx_stm32g474_adc_state_t* p) {
    if (detach(p) != NX_SUCCESS)
        return NX_ERROR_IO;
    if ((NX_G474_READ(&p->registers->CR) & ADC_CR_ADEN) != 0U) {
        NX_G474_WRITE(&p->registers->CR,
                      NX_G474_READ(&p->registers->CR) | ADC_CR_ADDIS);
        if (wait_cr(p, ADC_CR_ADEN | ADC_CR_ADDIS, 0U) != NX_SUCCESS)
            return NX_ERROR_IO;
    }
    if ((NX_G474_READ(&p->registers->CR) & ADC_CR_ADCAL) != 0U)
        return NX_ERROR_BUSY;
    NX_G474_WRITE(&p->registers->CFGR, 0U);
    nx_arch_dsb();
    return NX_G474_READ(&p->registers->CFGR) == 0U ? NX_SUCCESS : NX_ERROR_IO;
}
nx_result_t nx_stm32g474_adc_close(nx_stm32g474_adc_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->active)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32g474_adc_quiesce(p);
    if (result != NX_SUCCESS)
        return result;
    NX_G474_WRITE(&p->registers->CR, ADC_CR_DEEPPWD);
    NX_G474_WRITE(&p->common->CCR, 0U);
    nx_arch_dsb();
    if (NX_G474_READ(&p->registers->CR) != ADC_CR_DEEPPWD ||
        NX_G474_READ(&p->common->CCR) != 0U)
        return NX_ERROR_IO;
    p->initialized = false;
    p->retained = false;
    p->fault = false;
    return NX_SUCCESS;
}
static nx_result_t info(const void* context, nx_adc_info_t* output) {
    const nx_stm32g474_adc_state_t* p = context;
    if (!valid(p) || output == NULL || !p->initialized)
        return NX_ERROR_INVALID;
    *output = (nx_adc_info_t){12U, p->reference_mv, 1U};
    return NX_SUCCESS;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool span(const void* a, size_t n) {
    return a != NULL && (uintptr_t)a <= UINTPTR_MAX - n;
}
static bool protected(const nx_stm32g474_adc_state_t* p, const void* a,
                      size_t n) {
    return overlap(a, n, p, sizeof(*p)) ||
           overlap(a, n, p->registers, sizeof(*p->registers)) ||
           overlap(a, n, p->common, sizeof(*p->common)) ||
           overlap(a, n, p->peer, sizeof(*p->peer));
}
/** \brief Two consecutive conversions discard errata2.7.9/10's first result.
 * \note The caller holds the mask through this finite hardware sequence.
 *       Three nominal1MHz ticks retain at least2.65625 ADCcycles, above the
 *       SMP0 minimum2.5cycles and below the640.5-cycle maximum.
 */
nx_result_t nx_stm32g474_adc_prime(nx_stm32g474_adc_state_t* p,
                                   nx_time_us_t deadline, uint16_t* value) {
    nx_result_t result = NX_ERROR_TIMEOUT;
    if ((NX_G474_READ(&p->registers->CR) &
         (ADC_CR_ADEN | ADC_CR_ADSTART | ADC_CR_ADSTP)) != 0U)
        return NX_ERROR_BUSY;
    NX_G474_WRITE(&p->registers->CFGR, 0U);
    NX_G474_WRITE(&p->registers->ISR, ADC_ISR_ADRDY);
    nx_arch_dsb();
    if ((NX_G474_READ(&p->registers->ISR) & ADC_ISR_ADRDY) != 0U)
        return NX_ERROR_IO;
    NX_G474_WRITE(&p->registers->CR, ADC_CR_ADVREGEN | ADC_CR_ADEN);
    uint32_t ready_remaining = p->poll_limit;
    do {
        NX_G474_ADC_POLL(p);
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        if ((NX_G474_READ(&p->registers->ISR) & ADC_ISR_ADRDY) != 0U)
            break;
    } while (--ready_remaining != 0U);
    if (ready_remaining == 0U)
        return NX_ERROR_IO;
    for (unsigned shot = 0U; shot < 2U; ++shot) {
        if (shot != 0U && delay(p, 3U) != NX_SUCCESS)
            return NX_ERROR_TIMEOUT;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        NX_G474_WRITE(&p->registers->ISR,
                      ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR);
        nx_arch_dsb();
        if ((NX_G474_READ(&p->registers->ISR) &
             (ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR)) != 0U)
            return NX_ERROR_IO;
        NX_G474_WRITE(&p->registers->CR,
                      NX_G474_READ(&p->registers->CR) | ADC_CR_ADSTART);
        uint32_t remaining = p->poll_limit;
        result = NX_ERROR_TIMEOUT;
        do {
            NX_G474_ADC_POLL(p);
            if (nx_deadline_expired(deadline, nx_time_now_us()))
                break;
            uint32_t flags = NX_G474_READ(&p->registers->ISR);
            if ((flags & ADC_ISR_OVR) != 0U) {
                result = NX_ERROR_IO;
                break;
            }
            if ((flags & ADC_ISR_EOC) != 0U) {
                uint16_t raw = LL_ADC_REG_ReadConversionData12(p->registers);
                if (shot != 0U)
                    *value = raw;
                result = NX_SUCCESS;
                break;
            }
        } while (--remaining != 0U);
        if (result != NX_SUCCESS)
            break;
    }
    return result;
}
static nx_result_t sample(void* context, uint16_t* samples, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    nx_stm32g474_adc_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !span(samples, sizeof(*samples)) || capacity == 0U ||
        !span(count, sizeof(*count)) ||
        protected(p, samples, sizeof(*samples)) ||
        protected(p, count, sizeof(*count)) ||
        overlap(samples, sizeof(*samples), count, sizeof(*count)))
        return NX_ERROR_INVALID;
    *count = 0U;
    if (!p->initialized || p->fault)
        return NX_ERROR_STATE;
    if (p->active || (NX_G474_READ(&p->registers->CR) &
                      (ADC_CR_ADEN | ADC_CR_ADSTART)) != 0U)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    p->active = true;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_time_us_t bounded = nx_deadline_after(nx_time_now_us(), 100U);
    if (bounded > deadline)
        bounded = deadline;
    uint16_t value = 0U;
    nx_result_t result = nx_stm32g474_adc_prime(p, bounded, &value);
    if (nx_stm32g474_adc_quiesce(p) != NX_SUCCESS) {
        p->fault = true;
        (void)nx_arch_irq_save();
        nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
        for (;;) {
        }
    }
    if (result == NX_SUCCESS) {
        samples[0] = value;
        *count = 1U;
    }
    p->active = false;
    nx_arch_irq_restore(saved);
    return result;
}
const nx_adc_ops_t nx_stm32g474_adc_ops = {.info = info, .sample = sample};
