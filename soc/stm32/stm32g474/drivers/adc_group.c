/**
 * \file            adc_group.c
 * \brief           Exclusive finite ADC1 regular and injected sequence owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_adc_group.h"
#include "stm32g4xx_ll_adc.h"

#define AG_ADC_FLAGS                                                           \
    (ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR | ADC_ISR_JEOC |  \
     ADC_ISR_JEOS | ADC_ISR_AWD1)
#define AG_DMA_FLAGS (DMA_ISR_TCIF1 | DMA_ISR_HTIF1 | DMA_ISR_TEIF1)
#define AG_DMA_CONTROL                                                         \
    (DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | DMA_CCR_TCIE |         \
     DMA_CCR_TEIE)
enum { AG_IDLE, AG_PRIME, AG_PRIME_DRAIN, AG_RUN, AG_DRAIN };
enum { AG_TC = 1U, AG_JEOS = 2U };
static const uint16_t sample_halfcycles[8] = {5U,  13U,  25U,  49U,
                                              95U, 185U, 495U, 1281U};

static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* p, size_t bytes, size_t alignment) {
    return p != NULL && bytes != 0U && (uintptr_t)p % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief These known controls are rejected before reading a descriptor. */
static bool hardware_overlap(const void* a, size_t bytes) {
    return overlap(a, bytes, RCC, sizeof(*RCC)) ||
           overlap(a, bytes, GPIOA, sizeof(*GPIOA)) ||
           overlap(a, bytes, ADC1, sizeof(*ADC1)) ||
           overlap(a, bytes, ADC2, sizeof(*ADC2)) ||
           overlap(a, bytes, ADC12_COMMON, sizeof(*ADC12_COMMON)) ||
           overlap(a, bytes, DMA1, sizeof(*DMA1)) ||
           overlap(a, bytes, DMA1_Channel1, sizeof(*DMA1_Channel1)) ||
           overlap(a, bytes, DMAMUX1_Channel0, sizeof(*DMAMUX1_Channel0)) ||
           overlap(a, bytes, TIM6, sizeof(*TIM6)) ||
           overlap(a, bytes, DBGMCU, sizeof(*DBGMCU)) ||
           overlap(a, bytes, (const void*)(uintptr_t)0xE000E000U, 4096U) ||
           overlap(a, bytes, &g_nx_stm32g474_system,
                   sizeof(g_nx_stm32g474_system)) ||
           overlap(a, bytes, &nx_stm32g474_adc_group_ops,
                   sizeof(nx_stm32g474_adc_group_ops));
}
static bool valid(const nx_stm32g474_adc_group_state_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_stm32g474_adc_group_state_t)) &&
           !hardware_overlap(p, sizeof(*p)) &&
           span(p->config, sizeof(*p->config),
                _Alignof(nx_stm32g474_adc_group_config_t)) &&
           !hardware_overlap(p->config, sizeof(*p->config));
}
static bool regular(const nx_stm32g474_adc_group_config_t* c) {
    return c->group == NX_ADC_REGULAR;
}
static bool timed(const nx_stm32g474_adc_group_config_t* c) {
    return c->trigger == NX_ADC_TRIGGER_TIMER;
}
static size_t ranks(const nx_stm32g474_adc_group_config_t* c) {
    return c->channel_count * c->profile->rank_repeat;
}
static uint32_t scan_halfcycles(const nx_stm32g474_adc_group_config_t* c) {
    uint32_t sum = 0U;
    for (size_t i = 0U; i != c->channel_count; ++i) {
        sum += ((uint32_t)sample_halfcycles[c->sample_times[i]] + 25U) *
               c->profile->rank_repeat;
    }
    return sum;
}
/** \brief Only selected controls are accessed; injected never leases DMA. */
static bool accessible(const nx_stm32g474_adc_group_config_t* c) {
    if ((NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_ADC12EN) == 0U ||
        (NX_G474_READ(&RCC->AHB2RSTR) & RCC_AHB2RSTR_ADC12RST) != 0U) {
        return false;
    }
    if (regular(c) &&
        ((NX_G474_READ(&RCC->AHB1ENR) &
          (RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN)) !=
             (RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN) ||
         (NX_G474_READ(&RCC->AHB1RSTR) &
          (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMAMUX1RST)) != 0U)) {
        return false;
    }
    return !timed(c) ||
           ((NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM6EN) != 0U &&
            (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_TIM6RST) == 0U);
}
static bool clock_ready(void) {
    return g_nx_stm32g474_system.started &&
           g_nx_stm32g474_system.core_hz == 170000000U &&
           g_nx_stm32g474_system.rcc == RCC &&
           (NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_ADC12SEL) ==
               RCC_CCIPR_ADC12SEL_1;
}
static bool write_checked(volatile uint32_t* reg, uint32_t value) {
    NX_G474_WRITE(reg, value);
    nx_arch_dsb();
    return NX_G474_READ(reg) == value;
}
static nx_result_t delay(nx_stm32g474_adc_group_state_t* p, uint32_t us) {
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), us);
    uint32_t remaining = p->config->poll_limit;
    do {
        NX_G474_ADC_POLL(p);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_SUCCESS;
        }
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
static nx_result_t wait_cr(nx_stm32g474_adc_group_state_t* p, uint32_t mask,
                           uint32_t expected) {
    uint32_t remaining = p->config->poll_limit;
    do {
        NX_G474_ADC_POLL(p);
        if ((NX_G474_READ(&ADC1->CR) & mask) == expected) {
            return NX_SUCCESS;
        }
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
static uint32_t internal_sources(const nx_stm32g474_adc_group_config_t* c) {
    uint32_t selected = 0U;
    for (size_t i = 0U; i != c->channel_count; ++i) {
        if (c->channels[i] == 16U) {
            selected |= ADC_CCR_VSENSESEL;
        } else if (c->channels[i] == 17U) {
            selected |= ADC_CCR_VBATSEL;
        } else if (c->channels[i] == 18U) {
            selected |= ADC_CCR_VREFEN;
        }
    }
    return selected;
}
/** \brief Cold validation never substitutes declared cut for observed source.
 */
static nx_result_t config_valid(const nx_stm32g474_adc_group_config_t* c) {
    if (!span(c, sizeof(*c), _Alignof(nx_stm32g474_adc_group_config_t)) ||
        hardware_overlap(c, sizeof(*c)) || c->channel_count == 0U ||
        c->channel_count > 16U || !c->exclusive || c->poll_limit == 0U ||
        c->priority != 5U || c->reference_mv < 1620U ||
        c->reference_mv > 3600U ||
        (c->group != NX_ADC_REGULAR && c->group != NX_ADC_INJECTED) ||
        (c->trigger != NX_ADC_TRIGGER_SOFTWARE &&
         c->trigger != NX_ADC_TRIGGER_TIMER) ||
        !span(c->channels, c->channel_count, 1U) ||
        !span(c->sample_times, c->channel_count, 1U) ||
        hardware_overlap(c->channels, c->channel_count) ||
        hardware_overlap(c->sample_times, c->channel_count) ||
        !span(c->silicon_source, sizeof(*c->silicon_source),
              _Alignof(nx_silicon_source_t)) ||
        hardware_overlap(c->silicon_source, sizeof(*c->silicon_source)) ||
        !span(c->profile, sizeof(*c->profile),
              _Alignof(nx_stm32g474_adc_group_profile_t)) ||
        hardware_overlap(c->profile, sizeof(*c->profile)) ||
        !span(c->profile->policy, sizeof(*c->profile->policy),
              _Alignof(nx_silicon_admission_policy_t)) ||
        hardware_overlap(c->profile->policy, sizeof(*c->profile->policy)) ||
        c->profile->policy->allow_unreviewed ||
        (c->profile->rank_repeat != 1U && c->profile->rank_repeat != 2U) ||
        (c->watchdog &&
         (c->watchdog_low > c->watchdog_high || c->watchdog_high > 4095U))) {
        return NX_ERROR_INVALID;
    }
    if (ranks(c) > (regular(c) ? 16U : 4U)) {
        return NX_ERROR_UNSUPPORTED;
    }
    for (size_t i = 0U; i != c->channel_count; ++i) {
        uint8_t ch = c->channels[i], code = c->sample_times[i];
        if ((ch != 1U && ch != 2U && ch != 16U && ch != 17U && ch != 18U) ||
            code > 7U) {
            return NX_ERROR_INVALID;
        }
        if ((ch == 16U && code < 6U) || (ch == 17U && code < 7U) ||
            (ch == 18U && code < 5U)) {
            return NX_ERROR_UNSUPPORTED;
        }
        for (size_t j = 0U; j != i; ++j) {
            if (c->channels[j] == ch && c->sample_times[j] != code) {
                return NX_ERROR_INVALID;
            }
        }
    }
    if (timed(c)) {
        /* At21.25MHz one halfcycle costs4 HCLK cycles. Include maximum
         * asynchronous trigger latency and integer-microsecond margin. */
        uint32_t cycles = (scan_halfcycles(c) + 5U) * 4U;
        uint32_t prime_us = (scan_halfcycles(c) * 4U + 169U) / 170U;
        if (c->trigger_identity == 0U || c->period_us == 0U ||
            c->period_us >= 1000U - prime_us ||
            c->period_us * 170U <= cycles + 170U) {
            return NX_ERROR_UNSUPPORTED;
        }
    } else if (c->trigger_identity != 0U || c->period_us != 0U) {
        return NX_ERROR_INVALID;
    }
    if (regular(c) &&
        (c->region_count == 0U ||
         c->region_count > SIZE_MAX / sizeof(*c->regions) ||
         !span(c->regions, c->region_count * sizeof(*c->regions),
               _Alignof(nx_dma_memory_region_t)) ||
         hardware_overlap(c->regions, c->region_count * sizeof(*c->regions)))) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
/** \brief Compile real rank registers while neither group is started. */
static bool sequence(const nx_stm32g474_adc_group_config_t* c) {
    uint32_t sqr[4] = {0U, 0U, 0U, 0U};
    uint32_t jsqr = (uint32_t)ranks(c) - 1U, smpr[2] = {0U, 0U};
    for (size_t i = 0U; i != c->channel_count; ++i) {
        unsigned ch = c->channels[i];
        smpr[ch / 10U] |= (uint32_t)c->sample_times[i] << (3U * (ch % 10U));
        for (unsigned repeat = 0U; repeat != c->profile->rank_repeat;
             ++repeat) {
            size_t rank = i * c->profile->rank_repeat + repeat;
            if (!regular(c)) {
                jsqr |= (uint32_t)ch << (ADC_JSQR_JSQ1_Pos + 6U * rank);
            } else if (rank < 4U) {
                sqr[0] |= (uint32_t)ch << (6U + 6U * rank);
            } else if (rank < 9U) {
                sqr[1] |= (uint32_t)ch << (6U * (rank - 4U));
            } else if (rank < 14U) {
                sqr[2] |= (uint32_t)ch << (6U * (rank - 9U));
            } else {
                sqr[3] |= (uint32_t)ch << (6U * (rank - 14U));
            }
        }
    }
    sqr[0] |= regular(c) ? (uint32_t)ranks(c) - 1U : 0U;
    return write_checked(&ADC1->SMPR1, smpr[0]) &&
           write_checked(&ADC1->SMPR2, smpr[1]) &&
           write_checked(&ADC1->SQR1, sqr[0]) &&
           write_checked(&ADC1->SQR2, sqr[1]) &&
           write_checked(&ADC1->SQR3, sqr[2]) &&
           write_checked(&ADC1->SQR4, sqr[3]) &&
           write_checked(&ADC1->JSQR, regular(c) ? 0U : jsqr);
}
nx_result_t
nx_stm32g474_adc_group_initialize(nx_stm32g474_adc_group_state_t* p,
                                  const nx_stm32g474_adc_group_config_t* c) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_adc_group_state_t)) ||
        hardware_overlap(p, sizeof(*p))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = config_valid(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (overlap(p, sizeof(*p), c, sizeof(*c))) {
        return NX_ERROR_INVALID;
    }
    if (p->retained || p->initialized || p->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!accessible(c) || !clock_ready()) {
        return NX_ERROR_STATE;
    }
    nx_silicon_admission_t admission;
    result =
        nx_silicon_preflight(c->silicon_source, c->profile->policy, &admission);
    if (result != NX_SUCCESS) {
        return result;
    }
    const nx_silicon_admission_policy_t* policy = c->profile->policy;
    if (overlap(p, sizeof(*p), c->channels, c->channel_count) ||
        overlap(p, sizeof(*p), c->sample_times, c->channel_count) ||
        overlap(p, sizeof(*p), c->profile, sizeof(*c->profile)) ||
        overlap(p, sizeof(*p), c->silicon_source, sizeof(*c->silicon_source)) ||
        overlap(p, sizeof(*p), policy, sizeof(*policy)) ||
        (policy->rule_count != 0U &&
         overlap(p, sizeof(*p), policy->rules,
                 policy->rule_count * sizeof(*policy->rules))) ||
        (regular(c) && overlap(p, sizeof(*p), c->regions,
                               c->region_count * sizeof(*c->regions)))) {
        return NX_ERROR_INVALID;
    }
    if ((NX_G474_READ(&ADC1->CR) & ~(uint32_t)ADC_CR_DEEPPWD) != 0U ||
        (NX_G474_READ(&ADC2->CR) & ~(uint32_t)ADC_CR_DEEPPWD) != 0U ||
        NX_G474_READ(&ADC1->IER) != 0U ||
        NX_G474_READ(&ADC12_COMMON->CCR) != 0U ||
        !NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn) ||
        (regular(c) && (NX_G474_READ(&DMA1_Channel1->CCR) != 0U ||
                        NX_G474_READ(&DMA1_Channel1->CMAR) != 0U ||
                        NX_G474_READ(&DMA1_Channel1->CPAR) != 0U ||
                        NX_G474_READ(&DMA1_Channel1->CNDTR) != 0U ||
                        NX_G474_READ(&DMAMUX1_Channel0->CCR) != 0U ||
                        !NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn))) ||
        (timed(c) &&
         (NX_G474_READ(&TIM6->CR1) != 0U || NX_G474_READ(&TIM6->CR2) != 0U ||
          NX_G474_READ(&TIM6->DIER) != 0U))) {
        return NX_ERROR_BUSY;
    }
    p->config = c;
    p->saved_common = NX_G474_READ(&ADC12_COMMON->CCR);
    p->saved_priority[0] = (uint8_t)NX_G474_IRQ_PRIORITY(ADC1_2_IRQn);
    if (regular(c)) {
        p->saved_priority[1] =
            (uint8_t)NX_G474_IRQ_PRIORITY(DMA1_Channel1_IRQn);
    }
    p->retained = true;
    p->vectors_retained = true;
    p->fault = false;
    p->stopping = false;
    p->phase = AG_IDLE;
    /* The platform retains both original pad snapshots until cold close. */
    NX_G474_WRITE(&GPIOA->PUPDR, NX_G474_READ(&GPIOA->PUPDR) & ~15U);
    NX_G474_WRITE(&GPIOA->MODER, NX_G474_READ(&GPIOA->MODER) | 15U);
    nx_arch_dsb();
    if ((NX_G474_READ(&GPIOA->PUPDR) & 15U) != 0U ||
        (NX_G474_READ(&GPIOA->MODER) & 15U) != 15U) {
        return NX_ERROR_IO;
    }
    uint32_t sources = internal_sources(c);
    if (!write_checked(&ADC12_COMMON->CCR, LL_ADC_CLOCK_ASYNC_DIV8 | sources) ||
        !write_checked(&ADC1->CR, ADC_CR_ADVREGEN)) {
        return NX_ERROR_IO;
    }
    result = delay(p, (sources & ADC_CCR_VSENSESEL) != 0U ? 122U : 22U);
    if (result != NX_SUCCESS || !write_checked(&ADC1->DIFSEL, 0U)) {
        return result != NX_SUCCESS ? result : NX_ERROR_IO;
    }
    /* ADCAL0 alone cannot distinguish a refused command from an already
     * completed calibration with an unchanged factor. Observe fresh busy in
     * a short guard, then wait unmasked. If busy was missed, retain effects
     * and fail closed; neither CALFACT nor a nominal duration proves it. */
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    NX_G474_WRITE(&ADC1->CR, ADC_CR_ADVREGEN | ADC_CR_ADCAL);
    nx_arch_dsb();
    bool calibrating = (NX_G474_READ(&ADC1->CR) & ADC_CR_ADCAL) != 0U;
    nx_arch_irq_restore(guard);
    if (!calibrating) {
        return NX_ERROR_IO;
    }
    result = wait_cr(p, ADC_CR_ADCAL, 0U);
    if (result != NX_SUCCESS) {
        return result;
    }
    /* Two quantized microseconds exceed four ADC cycles at21.25MHz. */
    result = delay(p, 2U);
    if (result != NX_SUCCESS || !write_checked(&ADC1->CFGR, ADC_CFGR_JQDIS) ||
        !write_checked(&ADC1->CFGR2, 0U) || !sequence(c) ||
        !write_checked(&ADC1->IER, 0U)) {
        return result != NX_SUCCESS ? result : NX_ERROR_IO;
    }
    NX_G474_WRITE(&ADC1->ISR, AG_ADC_FLAGS);
    nx_arch_dsb();
    if ((NX_G474_READ(&ADC1->ISR) & AG_ADC_FLAGS) != 0U) {
        return NX_ERROR_IO;
    }
    p->initialized = true;
    return NX_SUCCESS;
}

/** \brief All cold descriptors remain protected throughout an accepted loan. */
static bool protected(const nx_stm32g474_adc_group_state_t* p, const void* a,
                      size_t bytes) {
    const nx_stm32g474_adc_group_config_t* c = p->config;
    const nx_silicon_admission_policy_t* policy = c->profile->policy;
    return hardware_overlap(a, bytes) || overlap(a, bytes, p, sizeof(*p)) ||
           overlap(a, bytes, c, sizeof(*c)) ||
           overlap(a, bytes, c->channels, c->channel_count) ||
           overlap(a, bytes, c->sample_times, c->channel_count) ||
           overlap(a, bytes, c->profile, sizeof(*c->profile)) ||
           overlap(a, bytes, c->silicon_source, sizeof(*c->silicon_source)) ||
           overlap(a, bytes, policy, sizeof(*policy)) ||
           (policy->rule_count != 0U &&
            overlap(a, bytes, policy->rules,
                    policy->rule_count * sizeof(*policy->rules))) ||
           (regular(c) && overlap(a, bytes, c->regions,
                                  c->region_count * sizeof(*c->regions)));
}
static nx_result_t info(const void* context, nx_adc_group_info_t* out) {
    const nx_stm32g474_adc_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !span(out, sizeof(*out), _Alignof(nx_adc_group_info_t)) ||
        protected(p, out, sizeof(*out)) ||
        (p->active != NULL &&
         (overlap(out, sizeof(*out), p->active, sizeof(*p->active)) ||
          overlap(out, sizeof(*out), p->active->frames, p->active->bytes)))) {
        return NX_ERROR_INVALID;
    }
    if (!p->initialized) {
        return NX_ERROR_STATE;
    }
    const nx_stm32g474_adc_group_config_t* c = p->config;
    const size_t physical_ranks = ranks(c);
    if (physical_ranks == 0U) {
        return NX_ERROR_STATE;
    }
    const size_t stride = (size_t)2U * c->profile->rank_repeat;
    *out = (nx_adc_group_info_t){
        .format = {.sample_stride = stride,
                   .scan_stride = stride * c->channel_count,
                   .trigger_identity = c->trigger_identity,
                   .channel_count = (uint16_t)c->channel_count,
                   .storage_bytes = 2U,
                   .significant_bits = 12U,
                   .lsb_bits = 0U,
                   .encoding = NX_ADC_UNSIGNED,
                   .byte_order = NX_ADC_LITTLE_ENDIAN,
                   .group = c->group,
                   .trigger = c->trigger},
        .channels = c->channels,
        .sample_times = c->sample_times,
        .sample_halfcycles = sample_halfcycles,
        .sample_code_count = 8U,
        .adc_clock_hz = 21250000U,
        .physical_scan_halfcycles = scan_halfcycles(c),
        .timer_period_us = c->period_us,
        .max_frames = regular(c) ? 65535U / physical_ranks : 1U,
        .physical_ranks = (uint16_t)physical_ranks,
        .active = p->active != NULL,
        .fault = p->fault,
        .quarantined = p->quarantined};
    return NX_SUCCESS;
}
/** \brief Preserve the first terminal cause; faults never revoke the loan. */
static void cause(nx_stm32g474_adc_group_state_t* p, nx_result_t result) {
    if (p->terminal == NX_SUCCESS) {
        p->terminal = result;
    }
    if (result == NX_ERROR_IO) {
        p->fault = true;
    }
}
/** \brief Snapshot captured errors before W1C, including the final drain. */
static void observe(nx_stm32g474_adc_group_state_t* p) {
    uint32_t adc = NX_G474_READ(&ADC1->ISR);
    if ((adc & ADC_ISR_OVR) != 0U ||
        ((adc & ADC_ISR_AWD1) != 0U &&
         (p->phase == AG_RUN || p->phase == AG_DRAIN))) {
        cause(p, NX_ERROR_IO);
    }
    if ((adc & ADC_ISR_JEOS) != 0U) {
        p->observed_flags |= AG_JEOS;
    }
    if (regular(p->config)) {
        uint32_t dma = NX_G474_READ(&DMA1->ISR);
        if ((dma & DMA_ISR_TEIF1) != 0U) {
            cause(p, NX_ERROR_IO);
        }
        if ((dma & DMA_ISR_TCIF1) != 0U) {
            p->observed_flags |= AG_TC;
        }
    }
    bool complete = regular(p->config)
                        ? (p->observed_flags & AG_TC) != 0U &&
                              NX_G474_READ(&DMA1_Channel1->CNDTR) == 0U
                        : (p->observed_flags & AG_JEOS) != 0U;
    if (p->phase == AG_RUN && complete && !p->completed) {
        if (nx_deadline_expired(p->active->base.deadline, nx_time_now_us())) {
            cause(p, NX_ERROR_TIMEOUT);
        }
        p->completed = true;
    }
}
/** \brief Discard pending source flags without erasing a captured failure. */
static bool acknowledge(nx_stm32g474_adc_group_state_t* p) {
    observe(p);
    uint32_t adc = NX_G474_READ(&ADC1->ISR) & AG_ADC_FLAGS;
    if ((adc & ADC_ISR_OVR) != 0U ||
        ((adc & ADC_ISR_AWD1) != 0U &&
         (p->phase == AG_RUN || p->phase == AG_DRAIN))) {
        cause(p, NX_ERROR_IO);
    }
    NX_G474_WRITE(&ADC1->ISR, adc);
    bool quiet = (NX_G474_READ(&ADC1->ISR) & AG_ADC_FLAGS) == 0U;
    if (regular(p->config)) {
        uint32_t dma = NX_G474_READ(&DMA1->ISR) & AG_DMA_FLAGS;
        if ((dma & DMA_ISR_TEIF1) != 0U) {
            cause(p, NX_ERROR_IO);
        }
        NX_G474_WRITE(&DMA1->IFCR, dma);
        quiet = (NX_G474_READ(&DMA1->ISR) & AG_DMA_FLAGS) == 0U && quiet;
    }
    observe(p);
    return quiet;
}
/** \brief One hardware poll per stop phase; no unbounded masked drain. */
static bool withdraw(nx_stm32g474_adc_group_state_t* p) {
    const nx_stm32g474_adc_group_config_t* c = p->config;
    if (!accessible(c)) {
        return false;
    }
    observe(p);
    bool timer = true;
    if (timed(c)) {
        timer = write_checked(&TIM6->CR1, 0U);
        /* MMS=enable must not be selected with CEN1: it would emit an edge. */
        timer = timer && write_checked(&TIM6->CR2, TIM_CR2_MMS_0);
    }
    bool interrupts = write_checked(&ADC1->IER, 0U);
    uint32_t cr = NX_G474_READ(&ADC1->CR);
    if ((cr & ADC_CR_ADSTART) != 0U && (cr & ADC_CR_ADSTP) == 0U) {
        NX_G474_WRITE(&ADC1->CR, cr | ADC_CR_ADSTP);
    }
    cr = NX_G474_READ(&ADC1->CR);
    if ((cr & ADC_CR_JADSTART) != 0U && (cr & ADC_CR_JADSTP) == 0U) {
        NX_G474_WRITE(&ADC1->CR, cr | ADC_CR_JADSTP);
    }
    NX_G474_ADC_POLL(p);
    observe(p);
    cr = NX_G474_READ(&ADC1->CR);
    bool stopped = (cr & (ADC_CR_ADSTART | ADC_CR_JADSTART | ADC_CR_ADSTP |
                          ADC_CR_JADSTP | ADC_CR_ADCAL)) == 0U;
    if (stopped && (cr & ADC_CR_ADEN) != 0U && (cr & ADC_CR_ADDIS) == 0U) {
        NX_G474_WRITE(&ADC1->CR, cr | ADC_CR_ADDIS);
    }
    NX_G474_ADC_POLL(p);
    observe(p);
    cr = NX_G474_READ(&ADC1->CR);
    bool adc = stopped && (cr & (ADC_CR_ADEN | ADC_CR_ADDIS | ADC_CR_ADSTART |
                                 ADC_CR_JADSTART | ADC_CR_ADSTP |
                                 ADC_CR_JADSTP | ADC_CR_ADCAL)) == 0U;
    bool routes = adc && write_checked(&ADC1->CFGR, ADC_CFGR_JQDIS) &&
                  write_checked(&ADC1->JSQR, 0U);
    bool dma = true;
    if (regular(c)) {
        dma = write_checked(&DMA1_Channel1->CCR, 0U);
        if (dma) {
            bool memory = write_checked(&DMA1_Channel1->CMAR, 0U);
            bool peripheral = write_checked(&DMA1_Channel1->CPAR, 0U);
            bool count = write_checked(&DMA1_Channel1->CNDTR, 0U);
            bool mux = write_checked(&DMAMUX1_Channel0->CCR, 0U);
            dma = memory && peripheral && count && mux;
        }
    }
    bool flags = acknowledge(p);
    nx_result_t adc_irq = NX_G474_IRQ_CONTROL(ADC1_2_IRQn, false, c->priority);
    bool vectors = adc_irq == NX_SUCCESS && NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn);
    if (regular(c)) {
        nx_result_t dma_irq =
            NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, false, c->priority);
        vectors = dma_irq == NX_SUCCESS &&
                  NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn) && vectors;
    }
    nx_arch_dsb();
    observe(p);
    return timer && interrupts && adc && routes && dma && flags && vectors;
}
static nx_result_t finish(nx_stm32g474_adc_group_state_t* p) {
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
    observe(p);
    nx_adc_group_request_t* request = p->active;
    if (request != NULL && p->terminal == NX_SUCCESS && !regular(p->config)) {
        /* A fresh JEOS owns all JDRs; source/vector drain precedes this copy.
         * Only valid logical ranks are copied, leaving explicit raw padding. */
        for (size_t i = 0U; i != p->config->channel_count; ++i) {
            size_t physical = i * p->config->profile->rank_repeat;
            volatile uint32_t* jdr =
                (volatile uint32_t*)((uintptr_t)&ADC1->JDR1 +
                                     physical * sizeof(uint32_t));
            uint16_t* frames = request->frames;
            frames[physical] = (uint16_t)NX_G474_READ(jdr);
        }
        observe(p);
    }
    nx_result_t result = p->terminal;
    size_t transferred =
        request != NULL && result == NX_SUCCESS ? request->frame_count : 0U;
    p->active = NULL;
    p->completed = false;
    p->quarantined = false;
    p->phase = AG_IDLE;
    p->observed_flags = 0U;
    if (request != NULL) {
        nx_request_settle(&request->base, result, transferred);
    }
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief A partial or padding-only span never becomes a valid frame. */
static nx_result_t request_valid(const nx_stm32g474_adc_group_state_t* p,
                                 const nx_adc_group_request_t* request) {
    if (!span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)) ||
        protected(p, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    const nx_stm32g474_adc_group_config_t* c = p->config;
    size_t physical = ranks(c), stride = 2U * physical;
    if (request->frame_count == 0U ||
        request->frame_count > (regular(c) ? 65535U / physical : 1U)) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (request->frame_count > SIZE_MAX / stride ||
        request->bytes != request->frame_count * stride ||
        !span(request->frames, request->bytes, _Alignof(uint16_t)) ||
        protected(p, request->frames, request->bytes) ||
        overlap(request, sizeof(*request), request->frames, request->bytes) ||
        request->base.deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (nx_request_state(&request->base) != NX_REQUEST_READY) {
        return NX_ERROR_STATE;
    }
    if (regular(c)) {
        nx_result_t result =
            nx_dma_buffer_validate(c->regions, c->region_count, request->frames,
                                   request->bytes, 2U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

static bool enable_adc(nx_stm32g474_adc_group_state_t* p) {
    if (NX_G474_READ(&ADC1->CR) != ADC_CR_ADVREGEN) {
        return false;
    }
    NX_G474_WRITE(&ADC1->ISR, ADC_ISR_ADRDY);
    nx_arch_dsb();
    if ((NX_G474_READ(&ADC1->ISR) & ADC_ISR_ADRDY) != 0U ||
        !write_checked(&ADC1->CR, ADC_CR_ADVREGEN | ADC_CR_ADEN)) {
        return false;
    }
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 10U);
    if (deadline > p->active->base.deadline) {
        deadline = p->active->base.deadline;
    }
    uint32_t remaining = p->config->poll_limit;
    do {
        NX_G474_ADC_POLL(p);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return false;
        }
        if ((NX_G474_READ(&ADC1->ISR) & ADC_ISR_ADRDY) != 0U) {
            return true;
        }
    } while (--remaining != 0U);
    return false;
}
/** \brief Every rank, including discarded erratum padding, uses real DMA. */
static bool dma_target(nx_stm32g474_adc_group_state_t* p, size_t beats) {
    return write_checked(&DMA1_Channel1->CCR, 0U) &&
           write_checked(&DMA1_Channel1->CPAR,
                         (uint32_t)(uintptr_t)&ADC1->DR) &&
           write_checked(&DMA1_Channel1->CMAR,
                         (uint32_t)(uintptr_t)p->active->frames) &&
           write_checked(&DMA1_Channel1->CNDTR, (uint32_t)beats) &&
           write_checked(&DMAMUX1_Channel0->CCR, 5U);
}
static bool arm_vectors(nx_stm32g474_adc_group_state_t* p) {
    if (NX_G474_IRQ_CONTROL(ADC1_2_IRQn, true, p->config->priority) !=
        NX_SUCCESS) {
        return false;
    }
    return !regular(p->config) ||
           NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, true, p->config->priority) ==
               NX_SUCCESS;
}
/** \brief A single discarded software scan avoids stale calibration/idle data.
 * \note The accepted caller span supplies its raw scratch; there is no hidden
 *       staging pool. Watchdog/periodic source are disabled during this scan.
 */
static bool prime_start(nx_stm32g474_adc_group_state_t* p) {
    const nx_stm32g474_adc_group_config_t* c = p->config;
    p->phase = AG_PRIME;
    p->completed = false;
    if (!sequence(c) ||
        !write_checked(&ADC1->CFGR,
                       ADC_CFGR_JQDIS | (regular(c) ? ADC_CFGR_DMAEN : 0U)) ||
        !write_checked(&ADC1->IER, 0U) || !acknowledge(p) ||
        p->terminal != NX_SUCCESS || (regular(c) && !dma_target(p, ranks(c))) ||
        !enable_adc(p)) {
        return false;
    }
    p->observed_flags = 0U;
    if (regular(c) &&
        !write_checked(&DMA1_Channel1->CCR, AG_DMA_CONTROL | DMA_CCR_EN)) {
        return false;
    }
    p->prime_started = nx_time_now_us();
    uint32_t command = regular(c) ? ADC_CR_ADSTART : ADC_CR_JADSTART;
    return write_checked(&ADC1->CR, ADC_CR_ADVREGEN | ADC_CR_ADEN | command);
}
/** \brief Quiet MMS prevents software UG from reaching the ADC synchronizer. */
static bool timer_prepare(const nx_stm32g474_adc_group_config_t* c) {
    uint32_t control = regular(c) ? 0U : TIM_CR1_OPM;
    if (!write_checked(&TIM6->CR1, control) ||
        !write_checked(&TIM6->CR2, TIM_CR2_MMS_0) ||
        !write_checked(&TIM6->DIER, 0U) || !write_checked(&TIM6->PSC, 169U) ||
        !write_checked(&TIM6->ARR, c->period_us - 1U)) {
        return false;
    }
    NX_G474_WRITE(&TIM6->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((NX_G474_READ(&TIM6->SR) & TIM_SR_UIF) != 0U) {
        return false;
    }
    NX_G474_WRITE(&TIM6->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if ((NX_G474_READ(&TIM6->SR) & TIM_SR_UIF) == 0U) {
        return false;
    }
    NX_G474_WRITE(&TIM6->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    return (NX_G474_READ(&TIM6->SR) & TIM_SR_UIF) == 0U &&
           NX_G474_READ(&TIM6->CNT) == 0U &&
           write_checked(&TIM6->CR2, TIM_CR2_MMS_1);
}
/** \brief The first actual trigger must follow a sufficiently recent prime. */
static bool prime_fresh(const nx_stm32g474_adc_group_state_t* p) {
    nx_time_us_t now = nx_time_now_us();
    nx_time_us_t first = nx_deadline_after(now, p->config->period_us);
    return now >= p->prime_started && first - p->prime_started < 1000U;
}
/** \brief The physical source starts last; software regular uses real CONT1. */
static bool run_start(nx_stm32g474_adc_group_state_t* p) {
    const nx_stm32g474_adc_group_config_t* c = p->config;
    if (!sequence(c) || (timed(c) && !timer_prepare(c)) || !acknowledge(p) ||
        p->terminal != NX_SUCCESS) {
        return false;
    }
    uint32_t cfgr = ADC_CFGR_JQDIS;
    uint32_t jsqr = NX_G474_READ(&ADC1->JSQR);
    if (regular(c)) {
        cfgr |= ADC_CFGR_DMAEN;
        cfgr |= timed(c) ? LL_ADC_REG_TRIG_EXT_TIM6_TRGO : ADC_CFGR_CONT;
        if (!dma_target(p, p->active->frame_count * ranks(c))) {
            return false;
        }
    } else if (timed(c)) {
        jsqr |= LL_ADC_INJ_TRIG_EXT_TIM6_TRGO;
    }
    uint32_t interrupts = ADC_IER_OVRIE;
    if (!regular(c)) {
        interrupts |= ADC_IER_JEOSIE;
    }
    if (c->watchdog) {
        cfgr |= regular(c) ? ADC_CFGR_AWD1EN : ADC_CFGR_JAWD1EN;
        interrupts |= ADC_IER_AWD1IE;
        if (!write_checked(&ADC1->TR1, ((uint32_t)c->watchdog_high << 16U) |
                                           c->watchdog_low)) {
            return false;
        }
    }
    if (!write_checked(&ADC1->CFGR, cfgr) ||
        !write_checked(&ADC1->JSQR, jsqr) ||
        !write_checked(&ADC1->IER, interrupts) || !enable_adc(p)) {
        return false;
    }
    p->observed_flags = 0U;
    p->completed = false;
    p->phase = AG_RUN;
    if (!arm_vectors(p) ||
        (regular(c) &&
         !write_checked(&DMA1_Channel1->CCR, AG_DMA_CONTROL | DMA_CCR_EN))) {
        return false;
    }
    uint32_t command = regular(c) ? ADC_CR_ADSTART : ADC_CR_JADSTART;
    if (!write_checked(&ADC1->CR, ADC_CR_ADVREGEN | ADC_CR_ADEN | command)) {
        return false;
    }
    if (!prime_fresh(p)) {
        /* Enabling ADC/vectors can be delayed after the earlier admission.
         * Keep the loan; the next service legally drains this arm and really
         * converts another discarded scan. Timed sources still have CEN0. */
        p->phase = AG_PRIME_DRAIN;
        return true;
    }
    return !timed(c) ||
           write_checked(&TIM6->CR1,
                         TIM_CR1_CEN | (regular(c) ? 0U : TIM_CR1_OPM));
}
static nx_result_t submit(void* context, nx_adc_group_request_t* request) {
    nx_stm32g474_adc_group_state_t* p = context;
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
    if (!accessible(p->config) || !clock_ready()) {
        return NX_ERROR_STATE;
    }
    if ((NX_G474_READ(&ADC1->CR) &
         (ADC_CR_ADEN | ADC_CR_ADSTART | ADC_CR_JADSTART | ADC_CR_ADSTP |
          ADC_CR_JADSTP | ADC_CR_ADDIS | ADC_CR_ADCAL)) != 0U ||
        !NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn) ||
        (regular(p->config) && (NX_G474_READ(&DMA1_Channel1->CCR) != 0U ||
                                !NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn))) ||
        (timed(p->config) && (NX_G474_READ(&TIM6->CR1) & TIM_CR1_CEN) != 0U)) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        p->active = request;
        p->terminal = NX_SUCCESS;
        p->completed = false;
        p->observed_flags = 0U;
    }
    nx_arch_irq_restore(saved);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!prime_start(p)) {
        cause(p, NX_ERROR_IO);
        p->phase = AG_DRAIN;
        (void)finish(p);
    }
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_adc_group_request_t* request) {
    nx_stm32g474_adc_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)) ||
        protected(p, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    if (p->active != request) {
        return nx_request_state(&request->base) == NX_REQUEST_SETTLED
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    cause(p, NX_ERROR_CANCELLED);
    p->phase = AG_DRAIN;
    (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
static nx_result_t service(void* context) {
    nx_stm32g474_adc_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    if (!p->initialized || !accessible(p->config)) {
        return NX_ERROR_STATE;
    }
    if (p->active == NULL) {
        return p->fault ? NX_ERROR_IO : NX_SUCCESS;
    }
    observe(p);
    if (!p->completed &&
        nx_deadline_expired(p->active->base.deadline, nx_time_now_us())) {
        cause(p, NX_ERROR_TIMEOUT);
    }
    if (p->terminal != NX_SUCCESS || (p->phase == AG_RUN && p->completed)) {
        p->phase = AG_DRAIN;
    }
    if (p->phase == AG_DRAIN) {
        return finish(p);
    }
    if (p->phase == AG_PRIME) {
        bool complete = regular(p->config)
                            ? (p->observed_flags & AG_TC) != 0U &&
                                  NX_G474_READ(&DMA1_Channel1->CNDTR) == 0U
                            : (p->observed_flags & AG_JEOS) != 0U;
        if (!complete) {
            NX_G474_ADC_POLL(p);
            return NX_ERROR_BUSY;
        }
        p->phase = AG_PRIME_DRAIN;
    }
    if (p->phase == AG_PRIME_DRAIN) {
        if (!withdraw(p)) {
            cause(p, NX_ERROR_IO);
            p->phase = AG_DRAIN;
            return finish(p);
        }
        if (p->terminal != NX_SUCCESS) {
            p->phase = AG_DRAIN;
            return finish(p);
        }
        /* prime_started is no later than the actual discarded conversion.
         * A late task must really convert again, not refresh a timestamp. */
        bool armed = prime_fresh(p) ? run_start(p) : prime_start(p);
        if (!armed) {
            cause(p, NX_ERROR_IO);
            p->phase = AG_DRAIN;
            return finish(p);
        }
        return NX_SUCCESS;
    }
    return NX_ERROR_BUSY;
}
static nx_result_t stop(void* context) {
    nx_stm32g474_adc_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_adc_group_state_t)) ||
        hardware_overlap(p, sizeof(*p))) {
        return NX_ERROR_INVALID;
    }
    /* A retired face cannot withdraw its successor's physical controller. */
    if (!p->retained) {
        return NX_SUCCESS;
    }
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    if (!accessible(p->config)) {
        return NX_ERROR_STATE;
    }
    p->stopping = true;
    if (p->active != NULL) {
        cause(p, NX_ERROR_CANCELLED);
        p->phase = AG_DRAIN;
        nx_result_t result = finish(p);
        return p->active != NULL              ? NX_ERROR_BUSY
               : result == NX_ERROR_CANCELLED ? NX_SUCCESS
                                              : result;
    }
    return withdraw(p) ? NX_SUCCESS : NX_ERROR_BUSY;
}
nx_result_t nx_stm32g474_adc_group_close(nx_stm32g474_adc_group_state_t* p) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_adc_group_state_t)) ||
        hardware_overlap(p, sizeof(*p))) {
        return NX_ERROR_INVALID;
    }
    if (!p->retained) {
        return NX_SUCCESS;
    }
    if (!valid(p) || !accessible(p->config)) {
        return NX_ERROR_STATE;
    }
    p->stopping = true;
    if (p->active != NULL) {
        cause(p, NX_ERROR_CANCELLED);
        p->phase = AG_DRAIN;
        (void)finish(p);
        if (p->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    if (!withdraw(p)) {
        return NX_ERROR_BUSY;
    }
    if (p->vectors_retained) {
        NX_G474_IRQ_PRIORITY_WRITE(ADC1_2_IRQn, p->saved_priority[0]);
        bool priorities =
            NX_G474_IRQ_PRIORITY(ADC1_2_IRQn) == p->saved_priority[0];
        if (regular(p->config)) {
            NX_G474_IRQ_PRIORITY_WRITE(DMA1_Channel1_IRQn,
                                       p->saved_priority[1]);
            priorities = NX_G474_IRQ_PRIORITY(DMA1_Channel1_IRQn) ==
                             p->saved_priority[1] &&
                         priorities;
        }
        if (!priorities) {
            return NX_ERROR_IO;
        }
        p->vectors_retained = false;
    }
    if (!write_checked(&ADC1->CFGR, 0U) || !write_checked(&ADC1->CFGR2, 0U) ||
        !write_checked(&ADC1->SQR1, 0U) || !write_checked(&ADC1->SQR2, 0U) ||
        !write_checked(&ADC1->SQR3, 0U) || !write_checked(&ADC1->SQR4, 0U) ||
        !write_checked(&ADC1->SMPR1, 0U) || !write_checked(&ADC1->SMPR2, 0U) ||
        !write_checked(&ADC1->CR, ADC_CR_DEEPPWD) ||
        !write_checked(&ADC12_COMMON->CCR, p->saved_common) ||
        (timed(p->config) &&
         (!write_checked(&TIM6->CR1, 0U) || !write_checked(&TIM6->CR2, 0U)))) {
        return NX_ERROR_IO;
    }
    p->initialized = false;
    p->retained = false;
    p->fault = false;
    p->quarantined = false;
    return NX_SUCCESS;
}
void nx_stm32g474_adc_group_adc_irq(nx_stm32g474_adc_group_state_t* p) {
    if (!nx_arch_is_privileged() || !nx_arch_in_isr() || !valid(p) ||
        !p->retained || p->active == NULL || !accessible(p->config)) {
        return;
    }
    observe(p);
    uint32_t flags =
        NX_G474_READ(&ADC1->ISR) & (ADC_ISR_OVR | ADC_ISR_JEOS | ADC_ISR_AWD1);
    if ((flags & ADC_ISR_OVR) != 0U ||
        ((flags & ADC_ISR_AWD1) != 0U && p->phase == AG_RUN)) {
        cause(p, NX_ERROR_IO);
    }
    if ((flags & ADC_ISR_JEOS) != 0U) {
        p->observed_flags |= AG_JEOS;
    }
    NX_G474_WRITE(&ADC1->ISR, flags);
    observe(p);
}
void nx_stm32g474_adc_group_dma_irq(nx_stm32g474_adc_group_state_t* p) {
    if (!nx_arch_is_privileged() || !nx_arch_in_isr() || !valid(p) ||
        !p->retained || p->active == NULL || !regular(p->config) ||
        !accessible(p->config)) {
        return;
    }
    observe(p);
    uint32_t flags = NX_G474_READ(&DMA1->ISR) & AG_DMA_FLAGS;
    if ((flags & DMA_ISR_TEIF1) != 0U) {
        cause(p, NX_ERROR_IO);
    }
    if ((flags & DMA_ISR_TCIF1) != 0U) {
        p->observed_flags |= AG_TC;
    }
    NX_G474_WRITE(&DMA1->IFCR, flags);
    observe(p);
}
const nx_adc_group_ops_t nx_stm32g474_adc_group_ops = {.submit = submit,
                                                       .cancel = cancel,
                                                       .service = service,
                                                       .stop = stop,
                                                       .info = info};
