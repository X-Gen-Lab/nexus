/**
 * \file            pwm_adc.c
 * \brief           Sole TIM1 / ADC1 finite DMA window and proven withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_pwm_adc.h"
#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_dmamux.h"

#ifndef NX_G474_PWM_ADC_TRIGGER_SELECT
#define NX_G474_PWM_ADC_TRIGGER_SELECT(adc, source)                            \
    LL_ADC_REG_SetTriggerSource((adc), (source))
#endif
#ifndef NX_G474_PWM_ADC_DMAMUX_SELECT
#define NX_G474_PWM_ADC_DMAMUX_SELECT(mux)                                     \
    LL_DMAMUX_SetRequestID((mux), LL_DMAMUX_CHANNEL_0, LL_DMAMUX_REQ_ADC1)
#endif
#define PA_COMPARATOR    (COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1)
#define PA_CHANNELS      (TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E)
#define PA_TRIGGER       (TIM_CR2_MMS2_2 | TIM_CR2_MMS2_1 | TIM_CR2_MMS2_0)
#define PA_QUIET_TRIGGER (TIM_CR2_MMS_0 | TIM_CR2_MMS2_0)
#define PA_DMA_FLAGS     (DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1)
#define PA_DMA_CONTROL                                                         \
    (DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | DMA_CCR_TCIE |         \
     DMA_CCR_TEIE)

/** \brief A unique privileged task owns all non-IRQ entry points. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Validate exclusive ends before descriptor reads and alias checks. */
static bool span(const void* p, size_t bytes, size_t alignment) {
    return p != NULL && bytes != 0U && (uintptr_t)p % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Caller descriptors must never reinterpret peripheral storage. */
static bool hardware_overlap(const void* a, size_t bytes) {
    return overlap(a, bytes, RCC, sizeof(*RCC)) ||
           overlap(a, bytes, GPIOA, sizeof(*GPIOA)) ||
           overlap(a, bytes, TIM1, sizeof(*TIM1)) ||
           overlap(a, bytes, TIM15, sizeof(*TIM15)) ||
           overlap(a, bytes, COMP1, sizeof(*COMP1)) ||
           overlap(a, bytes, ADC1, sizeof(*ADC1)) ||
           overlap(a, bytes, ADC2, sizeof(*ADC2)) ||
           overlap(a, bytes, ADC12_COMMON, sizeof(*ADC12_COMMON)) ||
           overlap(a, bytes, DMA1, sizeof(*DMA1)) ||
           overlap(a, bytes, DMA1_Channel1, sizeof(*DMA1_Channel1)) ||
           overlap(a, bytes, DMAMUX1_Channel0, sizeof(*DMAMUX1_Channel0)) ||
           overlap(a, bytes, (const void*)(uintptr_t)SCS_BASE, 4096U) ||
           overlap(a, bytes, &g_nx_stm32g474_system,
                   sizeof(g_nx_stm32g474_system));
}
/** \brief Only independently readable RCC facts precede peripheral access. */
static bool accessible(void) {
    const uint32_t ahb1 = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN;
    const uint32_t ahb2 = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_ADC12EN;
    const uint32_t apb2 = RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN;
    return (NX_G474_READ(&RCC->AHB1ENR) & ahb1) == ahb1 &&
           (NX_G474_READ(&RCC->AHB2ENR) & ahb2) == ahb2 &&
           (NX_G474_READ(&RCC->APB2ENR) & apb2) == apb2 &&
           (NX_G474_READ(&RCC->AHB1RSTR) &
            (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMAMUX1RST)) == 0U &&
           (NX_G474_READ(&RCC->AHB2RSTR) &
            (RCC_AHB2RSTR_GPIOARST | RCC_AHB2RSTR_ADC12RST)) == 0U &&
           (NX_G474_READ(&RCC->APB2RSTR) &
            (RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_SYSCFGRST)) == 0U;
}
/** \brief This selector never silently adopts another running clock profile. */
static bool profile(void) {
    return accessible() && g_nx_stm32g474_system.started &&
           g_nx_stm32g474_system.rcc == RCC &&
           g_nx_stm32g474_system.core_hz == 170000000U &&
           (NX_G474_READ(&RCC->CFGR) & RCC_CFGR_PPRE2) == 0U &&
           (NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_ADC12SEL) ==
               RCC_CCIPR_ADC12SEL_1 &&
           (NX_G474_READ(&GPIOA->MODER) & 0x003F030FU) ==
               (3U | (3U << 2U) | (3U << 8U) | (42U << 16U)) &&
           (NX_G474_READ(&GPIOA->AFR[1]) & 0xFFFU) == 0x666U &&
           (NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) ==
               PA_COMPARATOR;
}
static bool valid(const nx_stm32g474_pwm_adc_state_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_stm32g474_pwm_adc_state_t)) &&
           span(p->config, sizeof(*p->config),
                _Alignof(nx_stm32g474_pwm_adc_config_t));
}
/** \brief Every payload/output is separate from all retained cold metadata. */
static bool protected(const nx_stm32g474_pwm_adc_state_t* p, const void* a,
                      size_t bytes) {
    return overlap(a, bytes, p, sizeof(*p)) ||
           overlap(a, bytes, p->config, sizeof(*p->config)) ||
           overlap(a, bytes, p->config->regions,
                   p->config->region_count * sizeof(*p->config->regions)) ||
           overlap(a, bytes, &nx_stm32g474_pwm_adc_ops,
                   sizeof(nx_stm32g474_pwm_adc_ops)) ||
           hardware_overlap(a, bytes);
}
static bool write_checked(volatile uint32_t* address, uint32_t value) {
    NX_G474_WRITE(address, value);
    nx_arch_dsb();
    return NX_G474_READ(address) == value;
}
static uint32_t protection(const nx_stm32g474_pwm_adc_state_t* p) {
    return (uint32_t)p->pwm.deadtime_code | TIM_BDTR_OSSI | TIM_BDTR_OSSR |
           TIM_BDTR_BKE | TIM_BDTR_BKP;
}
/** \brief Physical faults override successful completion, never prior errors.
 */
static void fault(nx_stm32g474_pwm_adc_state_t* p) {
    p->fault = true;
    if (p->active != NULL && p->terminal == NX_SUCCESS) {
        p->terminal = NX_ERROR_IO;
    }
    p->completed = true;
}
/** \brief IRQ callers only latch facts and isolate the master output. */
static void observe_break(nx_stm32g474_pwm_adc_state_t* p) {
    if ((NX_G474_READ(&COMP1->CSR) & COMP_CSR_VALUE) != 0U ||
        (NX_G474_READ(&TIM1->SR) & TIM_SR_BIF) != 0U) {
        fault(p);
        if (!write_checked(&TIM1->BDTR, protection(p))) {
            p->quarantined = true;
        }
    }
}
/** \brief A prior TC never erases a later error during physical withdrawal. */
static void observe_faults(nx_stm32g474_pwm_adc_state_t* p) {
    observe_break(p);
    if ((NX_G474_READ(&ADC1->ISR) & ADC_ISR_OVR) != 0U ||
        (NX_G474_READ(&DMA1->ISR) & DMA_ISR_TEIF1) != 0U) {
        fault(p);
    }
}
/** \brief Read-only cold admission rejects foreign owners before effects. */
static bool cold_quiet(void) {
    return NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn) &&
           NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn) &&
           NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn) &&
           (NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_TIM15EN) == 0U &&
           (NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_ADC345EN) == 0U &&
           NX_G474_READ(&DMA1_Channel1->CCR) == 0U &&
           NX_G474_READ(&DMA1_Channel1->CNDTR) == 0U &&
           NX_G474_READ(&DMA1_Channel1->CPAR) == 0U &&
           NX_G474_READ(&DMA1_Channel1->CMAR) == 0U &&
           NX_G474_READ(&DMAMUX1_Channel0->CCR) == 0U &&
           NX_G474_READ(&TIM1->CR1) == 0U && NX_G474_READ(&TIM1->CR2) == 0U &&
           NX_G474_READ(&TIM1->SMCR) == 0U && NX_G474_READ(&TIM1->DIER) == 0U &&
           NX_G474_READ(&TIM1->CCER) == 0U &&
           NX_G474_READ(&TIM1->CCMR1) == 0U &&
           NX_G474_READ(&TIM1->CCMR2) == 0U &&
           NX_G474_READ(&TIM1->BDTR) == 0U && NX_G474_READ(&TIM1->AF1) == 0U &&
           NX_G474_READ(&TIM1->AF2) == 0U &&
           (NX_G474_READ(&TIM1->SR) & TIM_SR_BIF) == 0U &&
           (NX_G474_READ(&COMP1->CSR) & COMP_CSR_VALUE) == 0U &&
           (NX_G474_READ(&ADC1->CR) & ~(uint32_t)ADC_CR_DEEPPWD) == 0U &&
           (NX_G474_READ(&ADC2->CR) & ~(uint32_t)ADC_CR_DEEPPWD) == 0U &&
           NX_G474_READ(&ADC12_COMMON->CCR) == 0U &&
           NX_G474_READ(&ADC1->IER) == 0U;
}
nx_result_t
nx_stm32g474_pwm_adc_initialize(nx_stm32g474_pwm_adc_state_t* p,
                                const nx_stm32g474_pwm_adc_config_t* config) {
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_pwm_adc_state_t)) ||
        !span(config, sizeof(*config),
              _Alignof(nx_stm32g474_pwm_adc_config_t)) ||
        overlap(p, sizeof(*p), config, sizeof(*config)) ||
        hardware_overlap(p, sizeof(*p)) ||
        hardware_overlap(config, sizeof(*config)) ||
        config->region_count == 0U ||
        config->region_count > SIZE_MAX / sizeof(*config->regions) ||
        !span(config->regions, config->region_count * sizeof(*config->regions),
              _Alignof(nx_dma_memory_region_t)) ||
        overlap(p, sizeof(*p), config->regions,
                config->region_count * sizeof(*config->regions)) ||
        overlap(config, sizeof(*config), config->regions,
                config->region_count * sizeof(*config->regions)) ||
        hardware_overlap(config->regions,
                         config->region_count * sizeof(*config->regions)) ||
        config->reference_mv < 1620U || config->reference_mv > 3600U ||
        config->poll_limit == 0U || config->trigger_identity == 0U) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!config->exclusive) {
        return NX_ERROR_PERMISSION;
    }
    if (config->tick_hz != 1000000U || config->period_ticks != 100U ||
        config->trigger_ticks != 50U || config->deadtime_ticks != 170U) {
        return NX_ERROR_UNSUPPORTED;
    }
    for (size_t i = 0U; i != config->region_count; ++i) {
        const nx_dma_memory_region_t* region = &config->regions[i];
        if (region->size == 0U || region->begin < 0x20000000U ||
            region->begin >= 0x20018000U ||
            region->size > 0x20018000U - region->begin ||
            (region->permissions & NX_DMA_MEMORY_WRITE) == 0U) {
            return NX_ERROR_PERMISSION;
        }
    }
    if (p->retained || p->initialized || p->active != NULL ||
        p->config != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!profile()) {
        return NX_ERROR_STATE;
    }
    if (!cold_quiet()) {
        return NX_ERROR_BUSY;
    }
    p->config = config;
    p->retained = true;
    p->saved_priority[0] = (uint8_t)NX_G474_IRQ_PRIORITY(DMA1_Channel1_IRQn);
    p->saved_priority[1] = (uint8_t)NX_G474_IRQ_PRIORITY(TIM1_BRK_TIM15_IRQn);
    p->vectors_retained = true;
    p->adc = (nx_stm32g474_adc_state_t){.registers = ADC1,
                                        .peer = ADC2,
                                        .common = ADC12_COMMON,
                                        .reference_mv = config->reference_mv,
                                        .poll_limit = config->poll_limit};
    p->pwm_config = (nx_stm32g474_pwm_group_config_t){
        .tick_hz = config->tick_hz,
        .period_ticks = config->period_ticks,
        .deadtime_ticks = config->deadtime_ticks,
        .comparator_csr = PA_COMPARATOR,
        .exclusive = true};
    nx_result_t result =
        nx_stm32g474_pwm_group_initialize(&p->pwm, &p->pwm_config);
    if (result == NX_SUCCESS) {
        result = nx_stm32g474_adc_initialize(&p->adc);
    }
    if (result == NX_SUCCESS) {
        p->initialized = true;
    }
    return result;
}
/** \brief A quiet CEN precedes route changes and the fresh preload transfer. */
static bool load_frame(nx_stm32g474_pwm_adc_state_t* p,
                       const nx_pwm_group_frame_t* frame) {
    if (!write_checked(&TIM1->CR1, TIM_CR1_ARPE) ||
        !write_checked(&TIM1->CR2, PA_QUIET_TRIGGER) ||
        !write_checked(&TIM1->CCR1, frame->duty_ticks[0]) ||
        !write_checked(&TIM1->CCR2, frame->duty_ticks[1]) ||
        !write_checked(&TIM1->CCR3, frame->duty_ticks[2]) ||
        !write_checked(&TIM1->CCR4, frame->trigger_ticks)) {
        return false;
    }
    NX_G474_WRITE(&TIM1->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((NX_G474_READ(&TIM1->SR) & TIM_SR_UIF) != 0U) {
        return false;
    }
    NX_G474_WRITE(&TIM1->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if ((NX_G474_READ(&TIM1->SR) & TIM_SR_UIF) == 0U) {
        return false;
    }
    NX_G474_WRITE(&TIM1->SR, ~(uint32_t)TIM_SR_UIF);
    if ((NX_G474_READ(&TIM1->SR) & TIM_SR_UIF) != 0U ||
        !write_checked(&TIM1->CNT, 0U) ||
        !write_checked(&TIM1->CR2, PA_TRIGGER)) {
        return false;
    }
    for (size_t i = 0U; i != 3U; ++i) {
        p->pwm.duty_ticks[i] = frame->duty_ticks[i];
    }
    p->pwm.trigger_ticks = frame->trigger_ticks;
    return true;
}
/** \brief One poll per call bounds task work without a long masked drain. */
static bool withdraw(nx_stm32g474_pwm_adc_state_t* p) {
    if (!accessible()) {
        return false;
    }
    bool master = write_checked(&TIM1->BDTR, protection(p));
    bool channels = write_checked(&TIM1->CCER, 0U);
    bool interrupt = write_checked(&TIM1->DIER, 0U);
    bool counter = write_checked(&TIM1->CR1, TIM_CR1_ARPE | TIM_CR1_UDIS);
    /* MMS2=enable would create a rising edge if CEN were still asserted. */
    bool trigger = counter && write_checked(&TIM1->CR2, PA_QUIET_TRIGGER);
    uint32_t cr = NX_G474_READ(&ADC1->CR);
    if ((cr & ADC_CR_ADSTART) != 0U && (cr & ADC_CR_ADSTP) == 0U) {
        NX_G474_WRITE(&ADC1->CR, cr | ADC_CR_ADSTP);
    }
    NX_G474_ADC_POLL(&p->adc);
    cr = NX_G474_READ(&ADC1->CR);
    bool stopped = (cr & (ADC_CR_ADSTART | ADC_CR_ADSTP)) == 0U;
    if (stopped && (cr & ADC_CR_ADEN) != 0U && (cr & ADC_CR_ADDIS) == 0U) {
        NX_G474_WRITE(&ADC1->CR, cr | ADC_CR_ADDIS);
    }
    NX_G474_ADC_POLL(&p->adc);
    cr = NX_G474_READ(&ADC1->CR);
    bool adc = (cr & (ADC_CR_ADEN | ADC_CR_ADDIS | ADC_CR_ADSTART |
                      ADC_CR_ADSTP | ADC_CR_ADCAL)) == 0U;
    bool route = adc && write_checked(&ADC1->CFGR, 0U);
    bool dma = write_checked(&DMA1_Channel1->CCR, 0U);
    bool addresses = false;
    if (dma) {
        bool memory = write_checked(&DMA1_Channel1->CMAR, 0U);
        bool peripheral = write_checked(&DMA1_Channel1->CPAR, 0U);
        bool count = write_checked(&DMA1_Channel1->CNDTR, 0U);
        bool mux = write_checked(&DMAMUX1_Channel0->CCR, 0U);
        addresses = memory && peripheral && count && mux;
    }
    observe_faults(p);
    uint32_t observed = NX_G474_READ(&DMA1->ISR) & PA_DMA_FLAGS;
    if ((observed & DMA_ISR_TEIF1) != 0U) {
        fault(p);
    }
    NX_G474_WRITE(&DMA1->IFCR, observed);
    nx_result_t dma_irq = NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, false, 5U);
    nx_result_t break_irq = NX_G474_IRQ_CONTROL(TIM1_BRK_TIM15_IRQn, false, 5U);
    nx_arch_dsb();
    bool vectors = dma_irq == NX_SUCCESS && break_irq == NX_SUCCESS &&
                   NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn) &&
                   NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn);
    observe_faults(p);
    return master && channels && interrupt && counter && trigger && adc &&
           route && dma && addresses && vectors &&
           (NX_G474_READ(&DMA1->ISR) &
            (DMA_ISR_TCIF1 | DMA_ISR_HTIF1 | DMA_ISR_TEIF1)) == 0U;
}
/** \brief Keep first terminal cause while withdrawal retries retain the loan.
 */
static nx_result_t finish(nx_stm32g474_pwm_adc_state_t* p) {
    if (!withdraw(p)) {
        p->quarantined = true;
        p->fault = true;
        if (p->active != NULL) {
            (void)nx_request_transition(&p->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    /* Timer, ADC and DMA no longer produce data. Preserve the last physical
     * error snapshot before detaching the unique request and publishing it. */
    observe_faults(p);
    nx_pwm_adc_request_t* request = p->active;
    nx_result_t result = p->terminal;
    size_t transferred =
        request != NULL && result == NX_SUCCESS ? request->sample_count : 0U;
    p->active = NULL;
    p->adc.active = false;
    p->quarantined = false;
    p->completed = false;
    if (request != NULL) {
        nx_request_settle(&request->base, result, transferred);
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Validate the complete selected request without reading samples. */
static nx_result_t request_valid(const nx_stm32g474_pwm_adc_state_t* p,
                                 const nx_pwm_adc_request_t* request) {
    if (!span(request, sizeof(*request), _Alignof(nx_pwm_adc_request_t))) {
        return NX_ERROR_INVALID;
    }
    if (protected(p, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    if (request->sample_count == 0U || request->sample_count > 65535U ||
        request->frame.channel_count != 3U ||
        request->frame.trigger_ticks != p->config->trigger_ticks ||
        request->base.deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t bytes = request->sample_count * sizeof(*request->samples);
    if (!span(request->samples, bytes, _Alignof(uint16_t)) ||
        !span(request->frame.duty_ticks, 3U * sizeof(uint32_t),
              _Alignof(uint32_t)) ||
        protected(p, request->samples, bytes) ||
        protected(p, request->frame.duty_ticks, 3U * sizeof(uint32_t)) ||
        overlap(request, sizeof(*request), request->samples, bytes) ||
        overlap(request, sizeof(*request), request->frame.duty_ticks,
                3U * sizeof(uint32_t)) ||
        overlap(request->samples, bytes, request->frame.duty_ticks,
                3U * sizeof(uint32_t))) {
        return NX_ERROR_INVALID;
    }
    if (nx_request_state(&request->base) != NX_REQUEST_READY) {
        return NX_ERROR_STATE;
    }
    for (size_t i = 0U; i != 3U; ++i) {
        if (request->frame.duty_ticks[i] > p->config->period_ticks) {
            return NX_ERROR_INVALID;
        }
    }
    nx_result_t result =
        nx_dma_buffer_validate(p->config->regions, p->config->region_count,
                               request->samples, bytes, 2U, NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}
/** \brief Every fallible arm operation belongs to an already accepted loan. */
static nx_result_t arm(nx_stm32g474_pwm_adc_state_t* p,
                       nx_pwm_adc_request_t* request) {
    if (!load_frame(p, &request->frame)) {
        return NX_ERROR_IO;
    }
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 100U);
    if (deadline > request->base.deadline) {
        deadline = request->base.deadline;
    }
    uint16_t discarded = 0U;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = nx_stm32g474_adc_prime(&p->adc, deadline, &discarded);
    nx_arch_irq_restore(saved);
    if (result != NX_SUCCESS) {
        return result;
    }
    NX_G474_WRITE(&ADC1->ISR, ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR);
    if (!write_checked(&DMA1_Channel1->CCR, 0U) ||
        !write_checked(&DMA1_Channel1->CPAR, (uint32_t)(uintptr_t)&ADC1->DR) ||
        !write_checked(&DMA1_Channel1->CMAR,
                       (uint32_t)(uintptr_t)request->samples) ||
        !write_checked(&DMA1_Channel1->CNDTR,
                       (uint32_t)request->sample_count)) {
        return NX_ERROR_IO;
    }
    NX_G474_WRITE(&DMA1->IFCR, PA_DMA_FLAGS);
    NX_G474_PWM_ADC_DMAMUX_SELECT(DMAMUX1_Channel0);
    nx_arch_dsb();
    if (NX_G474_READ(&DMAMUX1_Channel0->CCR) != 5U ||
        !write_checked(&ADC1->CFGR, ADC_CFGR_DMAEN)) {
        return NX_ERROR_IO;
    }
    NX_G474_PWM_ADC_TRIGGER_SELECT(ADC1, LL_ADC_REG_TRIG_EXT_TIM1_TRGO2);
    nx_arch_dsb();
    if (NX_G474_READ(&ADC1->CFGR) !=
            (ADC_CFGR_DMAEN | LL_ADC_REG_TRIG_EXT_TIM1_TRGO2) ||
        NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, true, 5U) != NX_SUCCESS ||
        NX_G474_IRQ_CONTROL(TIM1_BRK_TIM15_IRQn, true, 5U) != NX_SUCCESS ||
        !write_checked(&DMA1_Channel1->CCR, PA_DMA_CONTROL | DMA_CCR_EN)) {
        return NX_ERROR_IO;
    }
    NX_G474_WRITE(&ADC1->CR, ADC_CR_ADVREGEN | ADC_CR_ADEN | ADC_CR_ADSTART);
    nx_arch_dsb();
    if (NX_G474_READ(&ADC1->CR) !=
        (ADC_CR_ADVREGEN | ADC_CR_ADEN | ADC_CR_ADSTART)) {
        return NX_ERROR_IO;
    }
    saved = nx_arch_irq_save();
    observe_break(p);
    bool ready =
        !p->fault &&
        !nx_deadline_expired(request->base.deadline, nx_time_now_us()) &&
        write_checked(&TIM1->CCER, PA_CHANNELS) &&
        write_checked(&TIM1->DIER, TIM_DIER_BIE) &&
        write_checked(&TIM1->CR1, TIM_CR1_ARPE | TIM_CR1_CEN);
    observe_break(p);
    ready = ready && !p->fault &&
            write_checked(&TIM1->BDTR, protection(p) | TIM_BDTR_MOE);
    observe_break(p);
    nx_arch_irq_restore(saved);
    return ready && !p->fault ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t submit(void* context, nx_pwm_adc_request_t* request) {
    nx_stm32g474_pwm_adc_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    if (!p->initialized || p->stopping || p->fault || p->quarantined) {
        return NX_ERROR_STATE;
    }
    if (p->active != NULL) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = request_valid(p, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!profile()) {
        return NX_ERROR_STATE;
    }
    if ((NX_G474_READ(&TIM1->CR1) & TIM_CR1_CEN) != 0U ||
        (NX_G474_READ(&TIM1->BDTR) & TIM_BDTR_MOE) != 0U ||
        NX_G474_READ(&DMA1_Channel1->CCR) != 0U ||
        !NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn) ||
        !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn)) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        p->active = request;
        p->adc.active = true;
        p->terminal = NX_SUCCESS;
        p->completed = false;
    }
    nx_arch_irq_restore(saved);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = arm(p, request);
    if (result != NX_SUCCESS) {
        saved = nx_arch_irq_save();
        if (p->terminal == NX_SUCCESS) {
            p->terminal = result;
        }
        p->completed = true;
        if (result == NX_ERROR_IO) {
            p->fault = true;
        }
        nx_arch_irq_restore(saved);
        (void)finish(p);
    }
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_pwm_adc_request_t* request) {
    nx_stm32g474_pwm_adc_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || request == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (p->active != request) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (!p->completed) {
        p->terminal = NX_ERROR_CANCELLED;
        p->completed = true;
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
static nx_result_t service(void* context) {
    nx_stm32g474_pwm_adc_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !p->retained) {
        return NX_ERROR_STATE;
    }
    if (!accessible()) {
        p->quarantined = true;
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    observe_break(p);
    if (p->active != NULL) {
        if ((NX_G474_READ(&ADC1->ISR) & ADC_ISR_OVR) != 0U ||
            (NX_G474_READ(&DMA1->ISR) & DMA_ISR_TEIF1) != 0U) {
            fault(p);
        } else if (!p->completed &&
                   nx_deadline_expired(p->active->base.deadline,
                                       nx_time_now_us())) {
            p->terminal = NX_ERROR_TIMEOUT;
            p->completed = true;
        } else if (!p->completed &&
                   (NX_G474_READ(&DMA1->ISR) & DMA_ISR_TCIF1) != 0U &&
                   NX_G474_READ(&DMA1_Channel1->CNDTR) == 0U) {
            p->completed = true;
        }
    }
    bool done = p->completed || p->stopping;
    if (done && p->active != NULL &&
        nx_request_state(&p->active->base) == NX_REQUEST_ACTIVE) {
        (void)nx_request_transition(&p->active->base, NX_REQUEST_DRAINING);
    }
    nx_arch_irq_restore(saved);
    return done ? finish(p) : NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32g474_pwm_adc_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !p->retained) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    p->stopping = true;
    if (p->active != NULL && !p->completed) {
        p->terminal = NX_ERROR_CANCELLED;
        p->completed = true;
    }
    nx_arch_irq_restore(saved);
    return service(p);
}
static nx_result_t state(const void* context, nx_pwm_adc_state_t* output) {
    const nx_stm32g474_pwm_adc_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !p->initialized ||
        !span(output, sizeof(*output), _Alignof(nx_pwm_adc_state_t)) ||
        protected(p, output, sizeof(*output))) {
        return NX_ERROR_INVALID;
    }
    if (p->active != NULL &&
        (overlap(output, sizeof(*output), p->active, sizeof(*p->active)) ||
         overlap(output, sizeof(*output), p->active->samples,
                 p->active->sample_count * sizeof(uint16_t)) ||
         overlap(output, sizeof(*output), p->active->frame.duty_ticks,
                 3U * sizeof(uint32_t)))) {
        return NX_ERROR_INVALID;
    }
    if (!profile()) {
        return NX_ERROR_STATE;
    }
    *output = (nx_pwm_adc_state_t){
        .format = {.sample_stride = 2U,
                   .scan_stride = 2U,
                   .trigger_identity = p->config->trigger_identity,
                   .channel_count = 1U,
                   .storage_bytes = 2U,
                   .significant_bits = 12U,
                   .encoding = NX_ADC_UNSIGNED,
                   .byte_order = NX_ADC_LITTLE_ENDIAN,
                   .group = NX_ADC_REGULAR,
                   .trigger = NX_ADC_TRIGGER_TIMER},
        .tick_hz = p->config->tick_hz,
        .period_ticks = p->config->period_ticks,
        .trigger_ticks = p->config->trigger_ticks,
        .deadtime_ticks = p->config->deadtime_ticks,
        .deadtime_hz = 170000000U,
        .pwm_channels = 3U,
        .max_samples = 65535U,
        .running = (NX_G474_READ(&TIM1->CR1) & TIM_CR1_CEN) != 0U,
        .fault = p->fault,
        .quarantined = p->quarantined};
    return NX_SUCCESS;
}
void nx_stm32g474_pwm_adc_dma_irq(nx_stm32g474_pwm_adc_state_t* p) {
    if (!valid(p) || !p->retained || !accessible()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t flags = NX_G474_READ(&DMA1->ISR);
    if (p->active != NULL) {
        if ((flags & DMA_ISR_TEIF1) != 0U ||
            (NX_G474_READ(&ADC1->ISR) & ADC_ISR_OVR) != 0U) {
            fault(p);
        } else if ((flags & DMA_ISR_TCIF1) != 0U &&
                   NX_G474_READ(&DMA1_Channel1->CNDTR) == 0U) {
            if (!p->completed && nx_deadline_expired(p->active->base.deadline,
                                                     nx_time_now_us())) {
                p->terminal = NX_ERROR_TIMEOUT;
            }
            p->completed = true;
        }
    }
    NX_G474_WRITE(&DMA1->IFCR, flags & PA_DMA_FLAGS);
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
}
void nx_stm32g474_pwm_adc_break_irq(nx_stm32g474_pwm_adc_state_t* p) {
    if (!valid(p) || !p->retained || !accessible()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    observe_break(p);
    NX_G474_WRITE(&TIM1->SR, ~(uint32_t)TIM_SR_BIF);
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
}
nx_result_t nx_stm32g474_pwm_adc_close(nx_stm32g474_pwm_adc_state_t* p) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_pwm_adc_state_t))) {
        return NX_ERROR_INVALID;
    }
    if (!p->retained) {
        return NX_SUCCESS;
    }
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = stop(p);
    if (result != NX_SUCCESS || p->active != NULL) {
        return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
    }
    if (p->adc.retained) {
        result = nx_stm32g474_adc_close(&p->adc);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    if (p->pwm.retained) {
        result = nx_stm32g474_pwm_group_close(&p->pwm);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    if (p->vectors_retained) {
        if (!NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn) ||
            !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn)) {
            return NX_ERROR_BUSY;
        }
        NX_G474_IRQ_PRIORITY_WRITE(DMA1_Channel1_IRQn, p->saved_priority[0]);
        NX_G474_IRQ_PRIORITY_WRITE(TIM1_BRK_TIM15_IRQn, p->saved_priority[1]);
        if (NX_G474_IRQ_PRIORITY(DMA1_Channel1_IRQn) != p->saved_priority[0] ||
            NX_G474_IRQ_PRIORITY(TIM1_BRK_TIM15_IRQn) != p->saved_priority[1]) {
            return NX_ERROR_IO;
        }
        p->vectors_retained = false;
    }
    *p = (nx_stm32g474_pwm_adc_state_t){0};
    return NX_SUCCESS;
}
const nx_pwm_adc_ops_t nx_stm32g474_pwm_adc_ops = {.submit = submit,
                                                   .cancel = cancel,
                                                   .service = service,
                                                   .stop = stop,
                                                   .state = state};
