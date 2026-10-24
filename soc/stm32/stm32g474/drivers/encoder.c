/**
 * \file            encoder.c
 * \brief           Exact TIM3 raw quadrature counter without hidden position
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_timer.h"

#define ENCODER_HZ       170000000U
#define ENCODER_CHANNELS (TIM_CCER_CC1E | TIM_CCER_CC2E)

/** \brief Thread controls are bounded and restore any incoming local mask. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr();
}
/** \brief Validate every extent before pointer arithmetic or config access. */
static bool span(const void* pointer, size_t size, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           size <= UINTPTR_MAX - (uintptr_t)pointer;
}
/** \brief This rejects wrapping extents as well as direct overlap. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (x < y + bn && y < x + an);
}
/** \brief Known mutable hardware cannot serve as config or caller output. */
static bool controls(const void* pointer, size_t size) {
    return overlap(pointer, size, TIM3, sizeof(*TIM3)) ||
           overlap(pointer, size, TIM5, sizeof(*TIM5)) ||
           overlap(pointer, size, GPIOA, sizeof(*GPIOA)) ||
           overlap(pointer, size, RCC, sizeof(*RCC)) ||
           overlap(pointer, size, (const void*)(uintptr_t)NVIC_BASE,
                   sizeof(NVIC_Type));
}
/** \brief Never adopt a stopped clock or another system's register mapping. */
static bool clock_ready(void) {
    return g_nx_stm32g474_system.started &&
           g_nx_stm32g474_system.core_hz == ENCODER_HZ &&
           g_nx_stm32g474_system.rcc == RCC &&
           (NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM3EN) != 0U &&
           (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_TIM3RST) == 0U &&
           (NX_G474_READ(&RCC->CFGR) & RCC_CFGR_PPRE1) == 0U &&
           (NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_GPIOAEN) != 0U;
}
/** \brief Pin routes remain Board-authorized facts through this cold lease. */
static bool pins_ready(void) {
    return (NX_G474_READ(&GPIOA->MODER) & (15U << 12U)) == (10U << 12U) &&
           (NX_G474_READ(&GPIOA->AFR[0]) & (255U << 24U)) == (34U << 24U);
}
/** \brief Reject unreadable ended sessions before accessing private config. */
static bool valid(const nx_stm32g474_encoder_state_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_stm32g474_encoder_state_t)) &&
           span(p->config, sizeof(*p->config),
                _Alignof(nx_stm32g474_encoder_config_t));
}
/** \brief Hardware polarity is independent on the two selected inputs. */
static uint32_t polarity(const nx_stm32g474_encoder_state_t* p) {
    return (p->config->invert_a ? TIM_CCER_CC1P : 0U) |
           (p->config->invert_b ? TIM_CCER_CC2P : 0U);
}
/** \brief Direct input mappings and exact filter codes never round. */
static uint32_t input_config(const nx_stm32g474_encoder_state_t* p) {
    return TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0 |
           ((uint32_t)p->config->filter_a << 4U) |
           ((uint32_t)p->config->filter_b << 12U);
}
/** \brief Every selected control write requires its own actual readback. */
static bool write(volatile uint32_t* address, uint32_t value) {
    NX_G474_WRITE(address, value);
    nx_arch_dsb();
    return NX_G474_READ(address) == value;
}
/** \brief Stop attempts every source even when one independent write fails. */
static bool withdraw(const nx_stm32g474_encoder_state_t* p) {
    bool interrupts = write(&TIM3->DIER, 0U);
    bool inputs = write(&TIM3->CCER, polarity(p));
    NX_G474_WRITE(&TIM3->CR1,
                  NX_G474_READ(&TIM3->CR1) & ~(uint32_t)TIM_CR1_CEN);
    nx_arch_dsb();
    bool counter = (NX_G474_READ(&TIM3->CR1) & TIM_CR1_CEN) == 0U;
    return interrupts && inputs && counter;
}
/** \brief Mutated clock2, index, NP, filter or base cannot masquerade as ready.
 */
static bool base_matches(const nx_stm32g474_encoder_state_t* p) {
    if (!clock_ready()) {
        return false;
    }
    uint32_t control = NX_G474_READ(&TIM3->CR1);
    uint32_t channels = polarity(p);
    if ((control & TIM_CR1_CEN) != 0U) {
        channels |= ENCODER_CHANNELS;
    }
    return pins_ready() &&
           (control & ~(uint32_t)(TIM_CR1_CEN | TIM_CR1_DIR)) == 0U &&
           NX_G474_READ(&TIM3->CR2) == 0U &&
           NX_G474_READ(&TIM3->SMCR) == (uint32_t)p->config->mode &&
           NX_G474_READ(&TIM3->DIER) == 0U &&
           NX_G474_READ(&TIM3->CCMR1) == input_config(p) &&
           NX_G474_READ(&TIM3->CCMR2) == 0U &&
           NX_G474_READ(&TIM3->CCER) == channels &&
           NX_G474_READ(&TIM3->PSC) == 0U &&
           NX_G474_READ(&TIM3->ARR) == UINT16_MAX;
}
/** \brief Validate and retain authority before the first controller effect. */
nx_result_t
nx_stm32g474_encoder_initialize(nx_stm32g474_encoder_state_t* p,
                                const nx_stm32g474_encoder_config_t* config) {
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_encoder_state_t)) ||
        !span(config, sizeof(*config),
              _Alignof(nx_stm32g474_encoder_config_t)) ||
        controls(p, sizeof(*p)) || controls(config, sizeof(*config)) ||
        overlap(p, sizeof(*p), config, sizeof(*config)) ||
        config->mode < NX_ENCODER_MODE_X2_TI1 ||
        config->mode > NX_ENCODER_MODE_X4_TI12 || config->filter_a > 15U ||
        config->filter_b > 15U) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!config->exclusive) {
        return NX_ERROR_PERMISSION;
    }
    if (p->config != NULL || p->initialized ||
        !NX_G474_IRQ_AVAILABLE(TIM3_IRQn)) {
        return NX_ERROR_BUSY;
    }
    if (!clock_ready() || !pins_ready()) {
        return NX_ERROR_STATE;
    }
    if (NX_G474_READ(&TIM3->CR1) != 0U || NX_G474_READ(&TIM3->CR2) != 0U ||
        NX_G474_READ(&TIM3->SMCR) != 0U || NX_G474_READ(&TIM3->DIER) != 0U ||
        NX_G474_READ(&TIM3->CCER) != 0U) {
        return NX_ERROR_BUSY;
    }
    p->config = config;
    if (!write(&TIM3->CR1, 0U) || !write(&TIM3->CR2, TIM_CR2_MMS_0) ||
        !write(&TIM3->SMCR, (uint32_t)config->mode) || !write(&TIM3->PSC, 0U) ||
        !write(&TIM3->ARR, UINT16_MAX) ||
        !write(&TIM3->CCMR1, input_config(p)) || !write(&TIM3->CCMR2, 0U) ||
        !write(&TIM3->CCER, polarity(p))) {
        return NX_ERROR_IO;
    }
    /* Quiet MMS follows disabled CEN while UG proves a fresh prescaler load.
     * ARR and CNT are physical16-bit facts; no software overflow extension. */
    NX_G474_WRITE(&TIM3->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((NX_G474_READ(&TIM3->SR) & TIM_SR_UIF) != 0U) {
        return NX_ERROR_IO;
    }
    NX_G474_WRITE(&TIM3->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if ((NX_G474_READ(&TIM3->SR) & TIM_SR_UIF) == 0U) {
        return NX_ERROR_IO;
    }
    NX_G474_WRITE(&TIM3->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((NX_G474_READ(&TIM3->SR) & TIM_SR_UIF) != 0U ||
        !write(&TIM3->CR2, 0U) || !base_matches(p)) {
        return NX_ERROR_IO;
    }
    p->initialized = true;
    return NX_SUCCESS;
}
/** \brief Start does not reset position or change the cold counting policy. */
static nx_result_t start(void* context) {
    nx_stm32g474_encoder_state_t* p = context;
    if (!valid(p) || !p->initialized) {
        return NX_ERROR_STATE;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!base_matches(p)) {
        return NX_ERROR_STATE;
    }
    if ((NX_G474_READ(&TIM3->CR1) & TIM_CR1_CEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    bool prepared = write(&TIM3->CCER, polarity(p) | ENCODER_CHANNELS);
    if (prepared) {
        NX_G474_WRITE(&TIM3->CR1, TIM_CR1_CEN);
        nx_arch_dsb();
        prepared = (NX_G474_READ(&TIM3->CR1) & TIM_CR1_CEN) != 0U;
    }
    if (!prepared) {
        (void)withdraw(p);
    }
    nx_arch_irq_restore(mask);
    return prepared ? NX_SUCCESS : NX_ERROR_IO;
}
/** \brief Refused withdrawal retains the immutable session and raw counter. */
static nx_result_t stop(void* context) {
    nx_stm32g474_encoder_state_t* p = context;
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!clock_ready()) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    bool stopped = withdraw(p);
    nx_arch_irq_restore(mask);
    return stopped ? NX_SUCCESS : NX_ERROR_BUSY;
}
/** \brief Output cannot overwrite any retained state/config/table/control. */
static bool output(const nx_stm32g474_encoder_state_t* p, const void* pointer,
                   size_t size, size_t alignment) {
    return span(pointer, size, alignment) && !controls(pointer, size) &&
           !overlap(pointer, size, p, sizeof(*p)) &&
           !overlap(pointer, size, p->config, sizeof(*p->config)) &&
           !overlap(pointer, size, &nx_stm32g474_encoder_ops,
                    sizeof(nx_stm32g474_encoder_ops));
}
/** \brief Read a raw modulo counter once; never infer a direction history. */
static nx_result_t read(void* context, nx_encoder_sample_t* sample) {
    const nx_stm32g474_encoder_state_t* p = context;
    if (!valid(p) ||
        !output(p, sample, sizeof(*sample), _Alignof(nx_encoder_sample_t))) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!p->initialized || !base_matches(p)) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    uint32_t value = NX_G474_READ(&TIM3->CNT) & UINT16_MAX;
    nx_time_us_t observed = nx_time_now_us();
    *sample = (nx_encoder_sample_t){.observed_at = observed, .count = value};
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Snapshot actual CEN separately from immutable filter/counter facts.
 */
static nx_result_t state(const void* context, nx_encoder_state_t* result) {
    const nx_stm32g474_encoder_state_t* p = context;
    if (!valid(p) ||
        !output(p, result, sizeof(*result), _Alignof(nx_encoder_state_t))) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!p->initialized || !base_matches(p)) {
        return NX_ERROR_STATE;
    }
    *result = (nx_encoder_state_t){
        .filter_clock_hz = ENCODER_HZ,
        .mode = p->config->mode,
        .counter_bits = 16U,
        .filter_a = p->config->filter_a,
        .filter_b = p->config->filter_b,
        .running = (NX_G474_READ(&TIM3->CR1) & TIM_CR1_CEN) != 0U};
    return NX_SUCCESS;
}
/** \brief Only an acquired stopped and publisher-joined session can end. */
nx_result_t nx_stm32g474_encoder_close(nx_stm32g474_encoder_state_t* p) {
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_encoder_state_t))) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (p->config == NULL) {
        return p->initialized ? NX_ERROR_STATE : NX_SUCCESS;
    }
    nx_result_t result = stop(p);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!NX_G474_IRQ_AVAILABLE(TIM3_IRQn)) {
        return NX_ERROR_BUSY;
    }
    /* DIR becomes writable only after the encoder personality is withdrawn.
     * Clear that ended direction before a new cold owner checks idle CR1. */
    if (!write(&TIM3->SMCR, 0U) || !write(&TIM3->CR1, 0U) ||
        !write(&TIM3->CCMR1, 0U) || !write(&TIM3->CCMR2, 0U) ||
        !write(&TIM3->CCER, 0U) || !write(&TIM3->CR2, 0U)) {
        return NX_ERROR_IO;
    }
    *p = (nx_stm32g474_encoder_state_t){0};
    return NX_SUCCESS;
}

const nx_encoder_ops_t nx_stm32g474_encoder_ops = {start, stop, read, state};
