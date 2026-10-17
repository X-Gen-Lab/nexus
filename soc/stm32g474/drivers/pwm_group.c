/**
 * \file            pwm_group.c
 * \brief           Coherent TIM1 group commits with latched COMP1 break
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_pwm_group.h"
#ifndef NX_G474_GROUP_TIM1
#define NX_G474_GROUP_TIM1 TIM1
#endif
#ifndef NX_G474_GROUP_COMP1
#define NX_G474_GROUP_COMP1 COMP1
#endif
#ifndef NX_G474_GROUP_RCC
#define NX_G474_GROUP_RCC RCC
#endif
#define GROUP_TIMER_HZ UINT32_C(170000000)
#define GROUP_CHANNELS (TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E)
#define GROUP_PWM1     (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE)
#define GROUP_PWM2                                                             \
    (TIM_CCMR2_OC4M_0 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4PE)
#define GROUP_TRIGGER       (TIM_CR2_MMS2_2 | TIM_CR2_MMS2_1 | TIM_CR2_MMS2_0)
#define GROUP_QUIET_TRIGGER (TIM_CR2_MMS_0 | TIM_CR2_MMS2_0)

/** \brief Cold/lifecycle operations never depend on progress with IRQs masked.
 */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Commit is bounded in Thread mode or a configurable peripheral IRQ. */
static bool commit_context(void) {
    uint32_t exception = nx_arch_exception_number();
    return nx_arch_is_privileged() && (exception == 0U || exception >= 16U);
}
/** \brief A complete exclusive-end span is checked before any access. */
static bool span(const void* pointer, size_t size, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    return pointer != NULL && size != 0U && start % alignment == 0U &&
           size <= UINTPTR_MAX - start;
}
/** \brief Checked exclusive ends make this alias check overflow-free. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Cold descriptors cannot reinterpret or overwrite owned registers. */
static bool hardware_overlap(const void* pointer, size_t size) {
    return overlap(pointer, size, NX_G474_GROUP_TIM1,
                   sizeof(*NX_G474_GROUP_TIM1)) ||
           overlap(pointer, size, NX_G474_GROUP_COMP1,
                   sizeof(*NX_G474_GROUP_COMP1)) ||
           overlap(pointer, size, NX_G474_GROUP_RCC,
                   sizeof(*NX_G474_GROUP_RCC));
}
/** \brief Every output and caller batch is separate from private ownership. */
static bool protected(const nx_stm32g474_pwm_group_state_t* p,
                      const void* pointer, size_t size) {
    return overlap(pointer, size, p, sizeof(*p)) ||
           overlap(pointer, size, p->config, sizeof(*p->config)) ||
           hardware_overlap(pointer, size);
}
/** \brief Exact deadtime representation; PSC never scales tDTS. */
static uint32_t deadtime(uint32_t code) {
    if (code < 128U) {
        return code;
    }
    if (code < 192U) {
        return (64U + (code & 63U)) * 2U;
    }
    if (code < 224U) {
        return (32U + (code & 31U)) * 8U;
    }
    return (32U + (code & 31U)) * 16U;
}
/** \brief The bounded cold search never appears in the IRQ commit path. */
static bool encode_deadtime(uint32_t ticks, uint8_t* code) {
    for (uint32_t candidate = 0U; candidate <= 255U; ++candidate) {
        if (deadtime(candidate) == ticks) {
            *code = (uint8_t)candidate;
            return true;
        }
    }
    return false;
}
/** \brief The active-high internal comparator cannot automatically reenable. */
static uint32_t protection(const nx_stm32g474_pwm_group_state_t* p) {
    return (uint32_t)p->deadtime_code | TIM_BDTR_OSSI | TIM_BDTR_OSSR |
           TIM_BDTR_BKE | TIM_BDTR_BKP;
}
/** \brief Readback is the source of success, never the attempted store. */
static bool write_checked(volatile uint32_t* address, uint32_t value) {
    NX_G474_WRITE(address, value);
    nx_arch_dsb();
    return NX_G474_READ(address) == value;
}
/** \brief Acknowledgment only targets the observed W0C source. */
static bool acknowledge(uint32_t flags) {
    NX_G474_WRITE(&NX_G474_GROUP_TIM1->SR, ~flags);
    nx_arch_dsb();
    return (NX_G474_READ(&NX_G474_GROUP_TIM1->SR) & flags) == 0U;
}
/** \brief Mutable session references always point to a checked cold config. */
static bool valid(const nx_stm32g474_pwm_group_state_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_stm32g474_pwm_group_state_t)) &&
           span(p->config, sizeof(*p->config),
                _Alignof(nx_stm32g474_pwm_group_config_t));
}
/** \brief Verify the established fixed profile and the independently owned
 * COMP. */
static bool clock_ready(const nx_stm32g474_pwm_group_state_t* p) {
    return g_nx_stm32g474_system.started &&
           g_nx_stm32g474_system.core_hz == GROUP_TIMER_HZ &&
           g_nx_stm32g474_system.rcc == NX_G474_GROUP_RCC &&
           (NX_G474_READ(&NX_G474_GROUP_RCC->APB2ENR) &
            (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN)) ==
               (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN) &&
           (NX_G474_READ(&NX_G474_GROUP_RCC->APB2RSTR) &
            (RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_SYSCFGRST)) == 0U &&
           (NX_G474_READ(&NX_G474_GROUP_RCC->CFGR) & RCC_CFGR_PPRE2) == 0U &&
           (NX_G474_READ(&NX_G474_GROUP_COMP1->CSR) &
            ~(uint32_t)COMP_CSR_VALUE) == p->config->comparator_csr;
}
/** \brief Comparator output already includes its owned input polarity. */
static bool break_active(void) {
    return (NX_G474_READ(&NX_G474_GROUP_COMP1->CSR) & COMP_CSR_VALUE) != 0U;
}
/** \brief One observed pulse latches fault even after the analog input clears.
 */
static void observe_break(nx_stm32g474_pwm_group_state_t* p) {
    if (break_active() ||
        (NX_G474_READ(&NX_G474_GROUP_TIM1->SR) & TIM_SR_BIF) != 0U) {
        p->fault = true;
        if (!write_checked(&NX_G474_GROUP_TIM1->BDTR, protection(p))) {
            p->quarantined = true;
        }
    }
}
/** \brief Independently try every withdrawal; keep UDIS while shadows may
 * differ. */
static bool quiet(nx_stm32g474_pwm_group_state_t* p) {
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    bool master = write_checked(&r->BDTR, protection(p));
    bool channels = write_checked(&r->CCER, 0U);
    bool counter = write_checked(&r->CR1, TIM_CR1_ARPE | TIM_CR1_UDIS);
    bool interrupt = write_checked(&r->DIER, 0U);
    bool trigger = write_checked(&r->CR2, GROUP_QUIET_TRIGGER);
    bool result = master && channels && counter && interrupt && trigger;
    p->quarantined = !result;
    return result;
}
/** \brief Failed partial shadow changes never become a live mixed frame. */
static nx_result_t failed(nx_stm32g474_pwm_group_state_t* p) {
    p->fault = true;
    (void)quiet(p);
    return NX_ERROR_IO;
}
/** \brief Fixed base, preload, route and all staged compare facts stay owned.
 */
static bool base_matches(const nx_stm32g474_pwm_group_state_t* p) {
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    uint32_t cr1 = NX_G474_READ(&r->CR1);
    bool running = (cr1 & TIM_CR1_CEN) != 0U;
    uint32_t cr2 = NX_G474_READ(&r->CR2);
    return (cr1 & ~(uint32_t)(TIM_CR1_CEN | TIM_CR1_UDIS)) == TIM_CR1_ARPE &&
           (cr1 & (TIM_CR1_CEN | TIM_CR1_UDIS)) !=
               (TIM_CR1_CEN | TIM_CR1_UDIS) &&
           NX_G474_READ(&r->PSC) == GROUP_TIMER_HZ / p->config->tick_hz - 1U &&
           NX_G474_READ(&r->ARR) == p->config->period_ticks - 1U &&
           NX_G474_READ(&r->SMCR) == 0U && NX_G474_READ(&r->RCR) == 0U &&
           (cr2 == GROUP_TRIGGER || (!running && cr2 == GROUP_QUIET_TRIGGER)) &&
           NX_G474_READ(&r->DIER) == (running ? TIM_DIER_BIE : 0U) &&
           NX_G474_READ(&r->CCER) == (running ? GROUP_CHANNELS : 0U) &&
           NX_G474_READ(&r->CCMR1) == (GROUP_PWM1 | (GROUP_PWM1 << 8U)) &&
           NX_G474_READ(&r->CCMR2) == (GROUP_PWM1 | GROUP_PWM2) &&
           NX_G474_READ(&r->AF1) == TIM1_AF1_BKCMP1E &&
           NX_G474_READ(&r->AF2) == 0U &&
           (NX_G474_READ(&r->BDTR) & ~(uint32_t)TIM_BDTR_MOE) ==
               protection(p) &&
           NX_G474_READ(&r->CCR1) == p->duty_ticks[0] &&
           NX_G474_READ(&r->CCR2) == p->duty_ticks[1] &&
           NX_G474_READ(&r->CCR3) == p->duty_ticks[2] &&
           NX_G474_READ(&r->CCR4) == p->trigger_ticks;
}
/** \brief No running counter or trigger can export the software reload. */
static bool load(nx_stm32g474_pwm_group_state_t* p) {
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    if (!write_checked(&r->CR2, GROUP_QUIET_TRIGGER) ||
        !write_checked(&r->CR1, TIM_CR1_ARPE) ||
        !write_checked(&r->CCR1, p->duty_ticks[0]) ||
        !write_checked(&r->CCR2, p->duty_ticks[1]) ||
        !write_checked(&r->CCR3, p->duty_ticks[2]) ||
        !write_checked(&r->CCR4, p->trigger_ticks)) {
        return false;
    }
    /* Clear an earlier event before UG, then require its fresh latch. An
     * attempted command cannot prove the preload actually became active. */
    if (!acknowledge(TIM_SR_UIF)) {
        return false;
    }
    NX_G474_WRITE(&r->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if ((NX_G474_READ(&r->SR) & TIM_SR_UIF) == 0U || !acknowledge(TIM_SR_UIF) ||
        !write_checked(&r->CNT, 0U) || !write_checked(&r->CR2, GROUP_TRIGGER)) {
        return false;
    }
    return true;
}
/** \brief Rejection has no hardware effects; failures retain the cold lease. */
nx_result_t nx_stm32g474_pwm_group_initialize(
    nx_stm32g474_pwm_group_state_t* p,
    const nx_stm32g474_pwm_group_config_t* config) {
    uint8_t code = 0U;
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_pwm_group_state_t)) ||
        !span(config, sizeof(*config),
              _Alignof(nx_stm32g474_pwm_group_config_t)) ||
        overlap(p, sizeof(*p), config, sizeof(*config)) ||
        hardware_overlap(p, sizeof(*p)) ||
        hardware_overlap(config, sizeof(*config)) || config->tick_hz == 0U ||
        GROUP_TIMER_HZ % config->tick_hz != 0U ||
        GROUP_TIMER_HZ / config->tick_hz > 65536U ||
        config->period_ticks < 2U || config->period_ticks > 65535U ||
        (config->comparator_csr & COMP_CSR_EN) == 0U ||
        (config->comparator_csr & (COMP_CSR_VALUE | COMP_CSR_LOCK)) != 0U ||
        !encode_deadtime(config->deadtime_ticks, &code)) {
        return NX_ERROR_INVALID;
    }
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!config->exclusive) {
        return NX_ERROR_PERMISSION;
    }
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    if (p->retained || p->initialized || p->config != NULL ||
        !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn) ||
        NX_G474_READ(&r->CR1) != 0U || NX_G474_READ(&r->CR2) != 0U ||
        NX_G474_READ(&r->SMCR) != 0U || NX_G474_READ(&r->DIER) != 0U ||
        NX_G474_READ(&r->CCER) != 0U || NX_G474_READ(&r->CCMR1) != 0U ||
        NX_G474_READ(&r->CCMR2) != 0U || NX_G474_READ(&r->BDTR) != 0U ||
        NX_G474_READ(&r->AF1) != 0U || NX_G474_READ(&r->AF2) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_stm32g474_pwm_group_state_t checked = {.config = config};
    if (!clock_ready(&checked)) {
        return NX_ERROR_STATE;
    }
    *p = (nx_stm32g474_pwm_group_state_t){.config = config,
                                          .trigger_ticks = 1U,
                                          .deadtime_code = code,
                                          .retained = true};
    if (!write_checked(&r->CR1, TIM_CR1_ARPE | TIM_CR1_UDIS) ||
        !write_checked(&r->PSC, GROUP_TIMER_HZ / config->tick_hz - 1U) ||
        !write_checked(&r->ARR, config->period_ticks - 1U) ||
        !write_checked(&r->RCR, 0U) ||
        !write_checked(&r->CCMR1, GROUP_PWM1 | (GROUP_PWM1 << 8U)) ||
        !write_checked(&r->CCMR2, GROUP_PWM1 | GROUP_PWM2) ||
        !write_checked(&r->BDTR, protection(p)) ||
        !write_checked(&r->AF1, TIM1_AF1_BKCMP1E) || !load(p)) {
        return failed(p);
    }
    p->initialized = true;
    observe_break(p);
    return p->quarantined ? NX_ERROR_IO : NX_SUCCESS;
}
/** \brief All values and aliases reject before UDIS or any compare write. */
static nx_result_t commit(void* context, const nx_pwm_group_frame_t* frame) {
    nx_stm32g474_pwm_group_state_t* p = context;
    if (!valid(p) ||
        !span(frame, sizeof(*frame), _Alignof(nx_pwm_group_frame_t)) ||
        frame->channel_count != 3U ||
        !span(frame->duty_ticks, 3U * sizeof(uint32_t), _Alignof(uint32_t)) ||
        protected(p, frame, sizeof(*frame)) ||
        protected(p, frame->duty_ticks, 3U * sizeof(uint32_t)) ||
        overlap(frame, sizeof(*frame), frame->duty_ticks,
                3U * sizeof(uint32_t)) ||
        frame->trigger_ticks == 0U ||
        frame->trigger_ticks >= p->config->period_ticks ||
        frame->duty_ticks[0] > p->config->period_ticks ||
        frame->duty_ticks[1] > p->config->period_ticks ||
        frame->duty_ticks[2] > p->config->period_ticks) {
        return NX_ERROR_INVALID;
    }
    if (!commit_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    observe_break(p);
    if (!p->initialized || p->fault || p->quarantined || !clock_ready(p) ||
        !base_matches(p)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    uint32_t previous = NX_G474_READ(&r->CR1);
    if (!write_checked(&r->CR1, previous | TIM_CR1_UDIS) ||
        !write_checked(&r->CCR1, frame->duty_ticks[0]) ||
        !write_checked(&r->CCR2, frame->duty_ticks[1]) ||
        !write_checked(&r->CCR3, frame->duty_ticks[2]) ||
        !write_checked(&r->CCR4, frame->trigger_ticks)) {
        nx_result_t result = failed(p);
        nx_arch_irq_restore(saved);
        return result;
    }
    if (!write_checked(&r->CR1, previous)) {
        nx_result_t result = failed(p);
        nx_arch_irq_restore(saved);
        return result;
    }
    p->duty_ticks[0] = frame->duty_ticks[0];
    p->duty_ticks[1] = frame->duty_ticks[1];
    p->duty_ticks[2] = frame->duty_ticks[2];
    p->trigger_ticks = frame->trigger_ticks;
    observe_break(p);
    nx_result_t result = p->fault ? NX_ERROR_IO : NX_SUCCESS;
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief Pending or active break is checked before and after MOE admission. */
static nx_result_t start(void* context) {
    nx_stm32g474_pwm_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !p->initialized) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    observe_break(p);
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    if (p->fault || p->quarantined || !clock_ready(p) || !base_matches(p)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if ((NX_G474_READ(&r->BDTR) & TIM_BDTR_MOE) != 0U ||
        NX_G474_READ(&r->CCER) != 0U ||
        (NX_G474_READ(&r->CR1) & TIM_CR1_CEN) != 0U ||
        NX_G474_READ(&r->DIER) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    bool prepared = load(p) && write_checked(&r->CCER, GROUP_CHANNELS) &&
                    write_checked(&r->DIER, TIM_DIER_BIE) &&
                    write_checked(&r->CR1, TIM_CR1_ARPE | TIM_CR1_CEN);
    observe_break(p);
    if (!prepared || p->fault ||
        !write_checked(&r->BDTR, protection(p) | TIM_BDTR_MOE)) {
        nx_result_t result = failed(p);
        nx_arch_irq_restore(saved);
        return result;
    }
    observe_break(p);
    nx_result_t result = p->fault ? failed(p) : NX_SUCCESS;
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief A failed source withdrawal never releases the private owner. */
static nx_result_t stop(void* context) {
    nx_stm32g474_pwm_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !p->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!clock_ready(p)) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    observe_break(p);
    bool result = quiet(p);
    if (!result) {
        p->fault = true;
    }
    nx_arch_irq_restore(saved);
    return result ? NX_SUCCESS : NX_ERROR_IO;
}
/** \brief Rebuild the last complete accepted group before clearing the latch.
 */
static nx_result_t clear_fault(void* context) {
    nx_stm32g474_pwm_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !p->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!clock_ready(p)) {
        return NX_ERROR_STATE;
    }
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if ((NX_G474_READ(&r->BDTR) & TIM_BDTR_MOE) != 0U ||
        NX_G474_READ(&r->CCER) != 0U ||
        (NX_G474_READ(&r->CR1) & TIM_CR1_CEN) != 0U || break_active()) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    if (!quiet(p) || !load(p) || !acknowledge(TIM_SR_BIF) || !base_matches(p)) {
        nx_result_t result = failed(p);
        nx_arch_irq_restore(saved);
        return result;
    }
    if (break_active() || (NX_G474_READ(&r->SR) & TIM_SR_BIF) != 0U) {
        p->fault = true;
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    p->fault = false;
    p->quarantined = false;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief State reports actual sources without editing caller-owned hardware.
 */
static nx_result_t state(const void* context, nx_pwm_group_state_t* output) {
    const nx_stm32g474_pwm_group_state_t* p = context;
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !p->initialized ||
        !span(output, sizeof(*output), _Alignof(nx_pwm_group_state_t)) ||
        protected(p, output, sizeof(*output))) {
        return NX_ERROR_INVALID;
    }
    if (!clock_ready(p)) {
        return NX_ERROR_STATE;
    }
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *output = (nx_pwm_group_state_t){
        .period_ticks = p->config->period_ticks,
        .tick_hz = p->config->tick_hz,
        .deadtime_ticks = p->config->deadtime_ticks,
        .deadtime_hz = GROUP_TIMER_HZ,
        .channel_count = 3U,
        .running = (NX_G474_READ(&r->BDTR) & TIM_BDTR_MOE) != 0U &&
                   (NX_G474_READ(&r->CR1) & TIM_CR1_CEN) != 0U &&
                   NX_G474_READ(&r->CCER) == GROUP_CHANNELS,
        .fault = p->fault || break_active() ||
                 (NX_G474_READ(&r->SR) & TIM_SR_BIF) != 0U,
        .quarantined = p->quarantined};
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief The hardware input clears MOE independently of IRQ scheduling. */
void nx_stm32g474_pwm_group_irq(nx_stm32g474_pwm_group_state_t* p) {
    if (!valid(p) || !p->retained) {
        return;
    }
    observe_break(p);
    if ((NX_G474_READ(&NX_G474_GROUP_TIM1->SR) & TIM_SR_BIF) != 0U &&
        !acknowledge(TIM_SR_BIF)) {
        p->quarantined = true;
    }
}
/** \brief Pins and comparator remain with cold assembly after timer close. */
nx_result_t nx_stm32g474_pwm_group_close(nx_stm32g474_pwm_group_state_t* p) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!span(p, sizeof(*p), _Alignof(nx_stm32g474_pwm_group_state_t))) {
        return NX_ERROR_INVALID;
    }
    if (!p->retained) {
        return NX_SUCCESS;
    }
    if (!valid(p) || !clock_ready(p)) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!quiet(p)) {
        p->fault = true;
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    if (!NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    TIM_TypeDef* r = NX_G474_GROUP_TIM1;
    if (!acknowledge(TIM_SR_BIF | TIM_SR_UIF) || !write_checked(&r->BDTR, 0U) ||
        !write_checked(&r->AF1, 0U) || !write_checked(&r->CCMR1, 0U) ||
        !write_checked(&r->CCMR2, 0U) || !write_checked(&r->CR2, 0U) ||
        !write_checked(&r->CR1, 0U)) {
        p->fault = true;
        p->quarantined = true;
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    *p = (nx_stm32g474_pwm_group_state_t){0};
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
const nx_pwm_group_ops_t nx_stm32g474_pwm_group_ops = {commit, start, stop,
                                                       clear_fault, state};
