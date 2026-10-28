/**
 * \file            adc.c
 * \brief           U575 ADC4 calibrated single-shot PC0 raw acquisition
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32u575_provider.h"
#include "stm32u5xx_ll_adc.h"
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32u575_adc_state_t* p) {
    return p != NULL && p->registers != NULL && p->common != NULL &&
           p->poll_limit != 0U && p->reference_mv >= 1000U &&
           p->reference_mv <= 3600U;
}
static bool clock_ready(void) {
    return g_nx_stm32u575_system.started &&
           g_nx_stm32u575_system.core_hz == 4000000U &&
           (NX_U575_READ(&NX_U575_RCC->AHB3ENR) & RCC_AHB3ENR_ADC4EN) != 0U &&
           (NX_U575_READ(&NX_U575_RCC->CCIPR3) & RCC_CCIPR3_ADCDACSEL) == 0U;
}
static nx_result_t wait(nx_stm32u575_adc_state_t* p, volatile uint32_t* reg,
                        uint32_t mask, uint32_t expected) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_U575_ADC_POLL(p);
        if ((NX_U575_READ(reg) & mask) == expected)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
static bool write_checked(volatile uint32_t* reg, uint32_t value) {
    NX_U575_WRITE(reg, value);
    nx_arch_dsb();
    return NX_U575_READ(reg) == value;
}
static bool clear_flags(ADC_TypeDef* r, uint32_t flags) {
    NX_U575_WRITE(&r->ISR, flags);
    nx_arch_dsb();
    return (NX_U575_READ(&r->ISR) & flags) == 0U;
}
static nx_result_t reset(nx_stm32u575_adc_state_t* p) {
    volatile uint32_t* reg = &NX_U575_RCC->AHB3RSTR;
    NX_U575_WRITE(reg, NX_U575_READ(reg) | RCC_AHB3RSTR_ADC4RST);
    nx_arch_dsb();
    if ((NX_U575_READ(reg) & RCC_AHB3RSTR_ADC4RST) == 0U)
        return NX_ERROR_IO;
    NX_U575_WRITE(reg, NX_U575_READ(reg) & ~(uint32_t)RCC_AHB3RSTR_ADC4RST);
    nx_arch_dsb();
    if ((NX_U575_READ(reg) & RCC_AHB3RSTR_ADC4RST) != 0U ||
        NX_U575_READ(&p->registers->CR) != 0U ||
        NX_U575_READ(&p->registers->IER) != 0U ||
        NX_U575_READ(&p->common->CCR) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_adc_prepare(nx_stm32u575_adc_state_t* p,
                                     const nx_stm32u575_adc_config_t* config) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (p == NULL || config == NULL)
        return NX_ERROR_INVALID;
    if (p->retained || p->initialized || p->active)
        return NX_ERROR_BUSY;
    p->registers = config->registers;
    p->common = config->common;
    p->reference_mv = config->reference_mv;
    p->poll_limit = config->poll_limit;
    return nx_stm32u575_adc_initialize(p);
}
nx_result_t nx_stm32u575_adc_initialize(nx_stm32u575_adc_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!clock_ready() ||
        (NX_U575_READ(&NX_U575_RCC->AHB2ENR1) & RCC_AHB2ENR1_ADC12EN) != 0U ||
        (NX_U575_READ(&NX_U575_RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0U ||
        (NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_ASV) == 0U)
        return NX_ERROR_STATE;
    ADC_TypeDef* r = p->registers;
    if (p->initialized || p->retained || p->active ||
        NX_U575_READ(&r->CR) != 0U || NX_U575_READ(&r->IER) != 0U ||
        NX_U575_READ(&r->CFGR1) != 0U || NX_U575_READ(&r->CFGR2) != 0U ||
        NX_U575_READ(&r->CHSELR) != 0U || NX_U575_READ(&p->common->CCR) != 0U)
        return NX_ERROR_BUSY;
    p->retained = true;
    p->fault = false;
    /* Platform retains PC0's original mux and pull until close succeeds. */
    NX_U575_WRITE(&GPIOC->MODER, NX_U575_READ(&GPIOC->MODER) | 3U);
    NX_U575_WRITE(&GPIOC->PUPDR, NX_U575_READ(&GPIOC->PUPDR) & ~3U);
    nx_arch_dsb();
    if ((NX_U575_READ(&GPIOC->MODER) & 3U) != 3U ||
        (NX_U575_READ(&GPIOC->PUPDR) & 3U) != 0U)
        return NX_ERROR_IO;
    if (!write_checked(&r->SMPR1, 7U) || !write_checked(&r->CHSELR, 2U))
        return NX_ERROR_IO;
    if (!clear_flags(r, ADC_ISR_LDORDY | ADC_ISR_ADRDY | ADC_ISR_EOC |
                            ADC_ISR_EOS | ADC_ISR_OVR))
        return NX_ERROR_IO;
    if (!write_checked(&r->CR, ADC_CR_ADVREGEN))
        return NX_ERROR_IO;
    nx_result_t result = wait(p, &r->ISR, ADC_ISR_LDORDY, ADC_ISR_LDORDY);
    if (result != NX_SUCCESS)
        return result;
    if (!write_checked(&r->CR, ADC_CR_ADVREGEN | ADC_CR_ADCAL))
        return NX_ERROR_IO;
    result = wait(p, &r->CR, ADC_CR_ADCAL, 0U);
    if (result != NX_SUCCESS)
        return result;
    /* RM0456 34.4.5 forbids ADEN for four ADC cycles after calibration.
     * This shared4MHz/1MHz domain waits two timer ticks, including the
     * unknown first-tick phase. Poll count is only a progress fail bound. */
    nx_time_us_t stable = nx_deadline_after(nx_time_now_us(), 2U);
    uint32_t remaining = p->poll_limit;
    while (!nx_deadline_expired(stable, nx_time_now_us())) {
        NX_U575_ADC_POLL(p);
        if (--remaining == 0U)
            return NX_ERROR_TIMEOUT;
    }
    if (!clear_flags(r, ADC_ISR_ADRDY))
        return NX_ERROR_IO;
    NX_U575_WRITE(&r->CR, ADC_CR_ADVREGEN | ADC_CR_ADEN);
    result = wait(p, &r->ISR, ADC_ISR_ADRDY, ADC_ISR_ADRDY);
    if (result == NX_SUCCESS &&
        (NX_U575_READ(&r->CR) & (ADC_CR_ADVREGEN | ADC_CR_ADEN)) ==
            (ADC_CR_ADVREGEN | ADC_CR_ADEN))
        p->initialized = true;
    else if (result == NX_SUCCESS)
        result = NX_ERROR_IO;
    return result;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool output(const nx_stm32u575_adc_state_t* p, const void* value,
                   size_t bytes) {
    return value != NULL && (uintptr_t)value <= UINTPTR_MAX - bytes &&
           !overlap(value, bytes, p, sizeof(*p)) &&
           !overlap(value, bytes, p->registers, sizeof(*p->registers)) &&
           !overlap(value, bytes, p->common, sizeof(*p->common));
}
static nx_result_t info(const void* context, nx_adc_info_t* out) {
    const nx_stm32u575_adc_state_t* p = context;
    if (!valid(p) || !output(p, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    if (!p->initialized ||
        LL_ADC_GetResolution(p->registers) != LL_ADC_RESOLUTION_12B)
        return NX_ERROR_STATE;
    *out = (nx_adc_info_t){12U, p->reference_mv, 1U};
    return NX_SUCCESS;
}
static nx_result_t sample(void* context, uint16_t* samples, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    nx_stm32u575_adc_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || capacity == 0U || !output(p, samples, sizeof(*samples)) ||
        !output(p, count, sizeof(*count)) ||
        (uintptr_t)samples % _Alignof(uint16_t) != 0U ||
        (uintptr_t)count % _Alignof(size_t) != 0U ||
        overlap(samples, sizeof(*samples), count, sizeof(*count)))
        return NX_ERROR_INVALID;
    if (!p->initialized || p->fault || !clock_ready())
        return NX_ERROR_STATE;
    if (p->active)
        return NX_ERROR_BUSY;
    *count = 0U;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    ADC_TypeDef* r = p->registers;
    if (NX_U575_READ(&r->CFGR1) != 0U || NX_U575_READ(&r->CFGR2) != 0U ||
        NX_U575_READ(&r->CHSELR) != 2U || NX_U575_READ(&r->SMPR1) != 7U ||
        (NX_U575_READ(&r->CR) & (ADC_CR_ADSTART | ADC_CR_ADSTP)) != 0U)
        return NX_ERROR_STATE;
    if (!clear_flags(r, ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR))
        return NX_ERROR_IO;
    p->active = true;
    NX_U575_WRITE(&r->CR, NX_U575_READ(&r->CR) | ADC_CR_ADSTART);
    nx_result_t result = NX_ERROR_TIMEOUT;
    uint32_t remaining = p->poll_limit;
    do {
        NX_U575_ADC_POLL(p);
        uint32_t status = NX_U575_READ(&r->ISR);
        if ((status & ADC_ISR_OVR) != 0U) {
            result = NX_ERROR_IO;
            break;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            break;
        if ((status & (ADC_ISR_EOC | ADC_ISR_EOS)) ==
            (ADC_ISR_EOC | ADC_ISR_EOS)) {
            samples[0] = (uint16_t)(NX_U575_READ(&r->DR) & 0xfffU);
            *count = 1U;
            result = NX_SUCCESS;
            break;
        }
    } while (--remaining != 0U);
    /* ADC4 requires ADSTP even after a completed software shot. */
    NX_U575_WRITE(&r->CR, NX_U575_READ(&r->CR) | ADC_CR_ADSTP);
    if (wait(p, &r->CR, ADC_CR_ADSTART | ADC_CR_ADSTP, 0U) != NX_SUCCESS) {
        p->fault = true;
        if (reset(p) != NX_SUCCESS) {
            (void)nx_arch_irq_save();
            nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__,
                             __LINE__);
            for (;;) {
            }
        }
        p->initialized = false;
        result = NX_ERROR_IO;
    }
    p->active = false;
    return result;
}
nx_result_t nx_stm32u575_adc_close(nx_stm32u575_adc_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!p->retained)
        return NX_SUCCESS;
    if (!clock_ready())
        return NX_ERROR_STATE;
    nx_result_t result = reset(p);
    if (result == NX_SUCCESS) {
        p->initialized = p->retained = p->fault = false;
    }
    return result;
}
const nx_adc_ops_t nx_stm32u575_adc_ops = {.info = info, .sample = sample};
