/**
 * \file            adc.c
 * \brief           H743 offset/linearity calibrated single conversion
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_provider.h"
#if NEXUS_CORE_HZ == 384000000U
#include "stm32h743_clock.h"
#endif
#include "stm32h7xx_ll_adc.h"

#ifndef NX_H743_SILICON_ID
#define NX_H743_SILICON_ID() (DBGMCU->IDCODE)
#endif
#define LINEAR_READY                                                           \
    (ADC_CR_LINCALRDYW1 | ADC_CR_LINCALRDYW2 | ADC_CR_LINCALRDYW3 |            \
     ADC_CR_LINCALRDYW4 | ADC_CR_LINCALRDYW5 | ADC_CR_LINCALRDYW6)
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32h743_adc_state_t* p) {
    return p != NULL && p->registers != NULL && p->peer != NULL &&
           p->common != NULL && p->gpio != NULL && p->poll_limit != 0U &&
           p->supply_mv >= 2400U && p->supply_mv <= 3300U &&
           p->reference_mv >= 2000U && p->reference_mv <= p->supply_mv;
}
static bool clock_ready(void) {
    return g_nx_stm32h743_system.started &&
#if NEXUS_CORE_HZ == 384000000U
           nx_stm32h743_pll384_clocks_ready() &&
#else
           (g_nx_stm32h743_system.core_hz == 32000000U ||
            g_nx_stm32h743_system.core_hz == 64000000U) &&
#endif
           g_nx_stm32h743_system.rcc != NULL &&
           (NX_H743_READ(&g_nx_stm32h743_system.rcc->AHB1ENR) &
            RCC_AHB1ENR_ADC12EN) != 0U;
}
static bool checked(volatile uint32_t* reg, uint32_t value) {
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return NX_H743_READ(reg) == value;
}
/** \brief Leave the six calibration-factor handshakes unchanged.
 * \note RM0433 permits only one enabled-idle calibration word handshake.
 *       Deep powerdown can discard analog calibration without CPU toggles.
 */
static bool control(nx_stm32h743_adc_state_t* p, uint32_t value) {
    value |= NX_H743_READ(&p->registers->CR) & LINEAR_READY;
    NX_H743_WRITE(&p->registers->CR, value);
    nx_arch_dsb();
    return (NX_H743_READ(&p->registers->CR) & ~LINEAR_READY) ==
           (value & ~LINEAR_READY);
}
/** \brief Prove a fresh completion boundary before enabling or starting.
 * \note An unobserved W1C may leave a previous conversion's ready flags.
 */
static bool clear_flags(nx_stm32h743_adc_state_t* p, uint32_t flags) {
    NX_H743_WRITE(&p->registers->ISR, flags);
    nx_arch_dsb();
    return (NX_H743_READ(&p->registers->ISR) & flags) == 0U;
}
static nx_result_t wait(nx_stm32h743_adc_state_t* p, volatile uint32_t* reg,
                        uint32_t mask, uint32_t expected,
                        nx_time_us_t deadline) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_H743_ADC_POLL(p);
        if ((NX_H743_READ(reg) & mask) == expected)
            return NX_SUCCESS;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool output(const nx_stm32h743_adc_state_t* p, const void* value,
                   size_t bytes, size_t alignment) {
    return value != NULL && (uintptr_t)value <= UINTPTR_MAX - bytes &&
           (uintptr_t)value % alignment == 0U &&
           !overlap(value, bytes, p, sizeof(*p)) &&
           !overlap(value, bytes, p->registers, sizeof(*p->registers)) &&
           !overlap(value, bytes, p->peer, sizeof(*p->peer)) &&
           !overlap(value, bytes, p->common, sizeof(*p->common)) &&
           !overlap(value, bytes, p->gpio, sizeof(*p->gpio));
}
nx_result_t nx_stm32h743_adc_initialize(nx_stm32h743_adc_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!clock_ready())
        return NX_ERROR_STATE;
    ADC_TypeDef* r = p->registers;
    if (p->initialized || p->retained || p->active ||
        (NX_H743_READ(&r->CR) & ~(uint32_t)(ADC_CR_DEEPPWD | LINEAR_READY)) !=
            0U ||
        (NX_H743_READ(&p->peer->CR) &
         ~(uint32_t)(ADC_CR_DEEPPWD | LINEAR_READY)) != 0U ||
        NX_H743_READ(&r->IER) != 0U ||
        (NX_H743_READ(&r->CFGR) & ~(uint32_t)ADC_CFGR_JQDIS) != 0U ||
        NX_H743_READ(&r->SQR1) != 0U || NX_H743_READ(&r->SQR2) != 0U ||
        NX_H743_READ(&r->SQR3) != 0U || NX_H743_READ(&r->SQR4) != 0U ||
        NX_H743_READ(&p->common->CCR) != 0U)
        return NX_ERROR_BUSY;
    p->saved_common = NX_H743_READ(&p->common->CCR);
    p->retained = true;
    p->fault = false;
    /* The synchronous HCLK/4 path avoids unowned PLL and ADC kernel muxes.
     * Official LL boost encoding preserves the cut1/cut2 distinction. */
#if NEXUS_CORE_HZ == 384000000U
    /* Joint owner selects per_ck/HSI64; acquire only this ADC divider.
     * The source envelope62..65MHz gives15.5..16.25MHz conversion clocks. */
    uint32_t boost = LL_ADC_BOOST_MODE_20MHZ;
    const uint32_t adc_clock = LL_ADC_CLOCK_ASYNC_DIV4;
#else
    uint32_t boost = g_nx_stm32h743_system.core_hz == 64000000U
                         ? LL_ADC_BOOST_MODE_20MHZ
                         : LL_ADC_BOOST_MODE_12MHZ5;
    const uint32_t adc_clock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
#endif
    boost = (NX_H743_SILICON_ID() & 0x30000000U) == 0x10000000U
                ? (boost >> 2U) & ADC_CR_BOOST_0
                : boost & ADC_CR_BOOST;
    if (!checked(&p->common->CCR, adc_clock) || !control(p, 0U) ||
        !control(p, ADC_CR_ADVREGEN | boost))
        return NX_ERROR_IO;
    nx_time_us_t ready = nx_deadline_after(nx_time_now_us(), 25U);
    uint32_t remaining = p->poll_limit;
    while (!nx_deadline_expired(ready, nx_time_now_us()) && remaining-- != 0U)
        NX_H743_ADC_POLL(p);
    if (!nx_deadline_expired(ready, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (!control(p, ADC_CR_ADVREGEN | boost | ADC_CR_ADCALLIN | ADC_CR_ADCAL))
        return NX_ERROR_IO;
    nx_arch_dsb();
    nx_result_t result = wait(p, &r->CR, ADC_CR_ADCAL, 0U, NX_DEADLINE_NEVER);
    if (result != NX_SUCCESS)
        return result;
    if ((NX_H743_READ(&r->CR) & LINEAR_READY) != LINEAR_READY ||
        !control(p, ADC_CR_ADVREGEN | boost) || !checked(&r->DIFSEL, 0U) ||
        !checked(&r->CFGR, ADC_CFGR_JQDIS) || !checked(&r->CFGR2, 0U) ||
        !checked(&r->SMPR2, 7U << ADC_SMPR2_SMP15_Pos) ||
        !checked(&r->PCSEL, 1U << 15U) ||
        !checked(&r->SQR1, 15U << ADC_SQR1_SQ1_Pos))
        return NX_ERROR_IO;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t fields = 3U << 6U;
    bool pins =
        checked(&p->gpio->PUPDR, NX_H743_READ(&p->gpio->PUPDR) & ~fields) &&
        checked(&p->gpio->MODER, NX_H743_READ(&p->gpio->MODER) | fields);
    nx_arch_irq_restore(saved);
    if (!pins)
        return NX_ERROR_IO;
    /* RM0433 requires four ADC clock cycles between calibration and enable.
     * Two nominal timer ticks cover phase quantization and four ADC cycles
     * at the selected8/16MHz synchronous clocks.
     */
    ready = nx_deadline_after(nx_time_now_us(), 2U);
    remaining = p->poll_limit;
    while (!nx_deadline_expired(ready, nx_time_now_us()) && remaining-- != 0U)
        NX_H743_ADC_POLL(p);
    if (!nx_deadline_expired(ready, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (!clear_flags(p, ADC_ISR_ADRDY))
        return NX_ERROR_IO;
    NX_H743_WRITE(&r->CR, NX_H743_READ(&r->CR) | ADC_CR_ADEN);
    nx_arch_dsb();
    uint32_t enabled = ADC_CR_ADEN | ADC_CR_ADVREGEN;
    if ((NX_H743_READ(&r->CR) & enabled) != enabled)
        return NX_ERROR_IO;
    result = wait(p, &r->ISR, ADC_ISR_ADRDY, ADC_ISR_ADRDY, NX_DEADLINE_NEVER);
    if (result == NX_SUCCESS)
        p->initialized = true;
    return result;
}
nx_result_t nx_stm32h743_adc_stop(nx_stm32h743_adc_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!clock_ready())
        return NX_ERROR_STATE;
    ADC_TypeDef* r = p->registers;
    if ((NX_H743_READ(&r->CR) & (ADC_CR_ADCAL | ADC_CR_JADSTART)) != 0U)
        return NX_ERROR_IO;
    if ((NX_H743_READ(&r->CR) & ADC_CR_ADSTART) != 0U) {
        NX_H743_WRITE(&r->CR, NX_H743_READ(&r->CR) | ADC_CR_ADSTP);
        if (wait(p, &r->CR, ADC_CR_ADSTART | ADC_CR_ADSTP, 0U,
                 NX_DEADLINE_NEVER) != NX_SUCCESS)
            return NX_ERROR_IO;
    }
    if ((NX_H743_READ(&r->CR) & ADC_CR_ADEN) != 0U) {
        NX_H743_WRITE(&r->CR, NX_H743_READ(&r->CR) | ADC_CR_ADDIS);
        if (wait(p, &r->CR, ADC_CR_ADEN | ADC_CR_ADDIS, 0U,
                 NX_DEADLINE_NEVER) != NX_SUCCESS)
            return NX_ERROR_IO;
    }
    if (!control(p, ADC_CR_DEEPPWD) || !checked(&r->SQR1, 0U) ||
        !checked(&r->PCSEL, 0U) || !checked(&r->SMPR2, 0U) ||
        !checked(&p->common->CCR, p->saved_common))
        return NX_ERROR_IO;
    p->retained = p->initialized = p->fault = false;
    return NX_SUCCESS;
}
static nx_result_t info(const void* context, nx_adc_info_t* out) {
    const nx_stm32h743_adc_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !output(p, out, sizeof(*out), _Alignof(nx_adc_info_t)))
        return NX_ERROR_INVALID;
    if (!p->initialized)
        return NX_ERROR_STATE;
    *out = (nx_adc_info_t){16U, p->reference_mv, 1U};
    return NX_SUCCESS;
}
static nx_result_t sample(void* context, uint16_t* samples, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    nx_stm32h743_adc_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || capacity == 0U ||
        !output(p, samples, sizeof(*samples), _Alignof(uint16_t)) ||
        !output(p, count, sizeof(*count), _Alignof(size_t)) ||
        overlap(samples, sizeof(*samples), count, sizeof(*count)))
        return NX_ERROR_INVALID;
    *count = 0U;
    if (!p->initialized || p->fault || !clock_ready())
        return NX_ERROR_STATE;
    if (p->active || (NX_H743_READ(&p->registers->CR) &
                      (ADC_CR_ADSTART | ADC_CR_JADSTART)) != 0U)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (!clear_flags(p, ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR)) {
        p->fault = true;
        return NX_ERROR_IO;
    }
    p->active = true;
    ADC_TypeDef* r = p->registers;
    NX_H743_WRITE(&r->CR, NX_H743_READ(&r->CR) | ADC_CR_ADSTART);
    nx_result_t result = wait(p, &r->ISR, ADC_ISR_EOC | ADC_ISR_EOS,
                              ADC_ISR_EOC | ADC_ISR_EOS, deadline);
    if ((NX_H743_READ(&r->ISR) & ADC_ISR_OVR) != 0U)
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS) {
        uint16_t value = (uint16_t)NX_H743_READ(&r->DR);
        NX_H743_WRITE(&r->ISR, ADC_ISR_EOC | ADC_ISR_EOS);
        if ((NX_H743_READ(&r->CR) & ADC_CR_ADSTART) != 0U)
            result = NX_ERROR_IO;
        else {
            samples[0] = value;
            *count = 1U;
        }
    }
    if (result != NX_SUCCESS)
        p->fault = true;
    p->active = false;
    return result;
}
const nx_adc_ops_t nx_stm32h743_adc_ops = {.info = info, .sample = sample};
