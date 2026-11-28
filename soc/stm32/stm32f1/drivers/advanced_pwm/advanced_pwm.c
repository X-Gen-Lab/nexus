/**
 * \file            advanced_pwm.c
 * \brief           STM32F103 TIM1 CH1 safe complementary output and break.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_tim.h"
#include "system/platform.h"

#define MAIN_PIN        UINT32_C(0x0100)
#define COMP_PIN        UINT32_C(0x2000)
#define BREAK_PIN       UINT32_C(0x1000)
#define MAIN_FIELD      (GPIO_CRH_MODE8 | GPIO_CRH_CNF8)
#define COMP_FIELD      (GPIO_CRH_MODE13 | GPIO_CRH_CNF13)
#define MAIN_IDLE       GPIO_CRH_MODE8_1
#define COMP_IDLE       GPIO_CRH_MODE13_1
#define MAIN_TIMER      (MAIN_IDLE | GPIO_CRH_CNF8_1)
#define COMP_TIMER      (COMP_IDLE | GPIO_CRH_CNF13_1)
#define BREAK_PULL      GPIO_CRH_CNF12_1
#define TIMER_HZ        UINT32_C(8000000)
#define OUTPUTS         (TIM_CCER_CC1E | TIM_CCER_CC1NE)
#define BASE_PROTECTION (TIM_BDTR_BKE | TIM_BDTR_OSSI | TIM_BDTR_OSSR)
#define PWM_MODE        (LL_TIM_OCMODE_PWM1 | TIM_CCMR1_OC1PE)

#ifndef NX_F1_TIM1_IRQ_ENABLED
#define NX_F1_TIM1_IRQ_ENABLED()   NVIC_GetEnableIRQ(TIM1_BRK_IRQn)
#define NX_F1_TIM1_IRQ_PENDING()   NVIC_GetPendingIRQ(TIM1_BRK_IRQn)
#define NX_F1_TIM1_IRQ_ACTIVE()    NVIC_GetActive(TIM1_BRK_IRQn)
#define NX_F1_TIM1_IRQ_PRIORITY()  NVIC_GetPriority(TIM1_BRK_IRQn)
#define NX_F1_TIM1_SET_PRIORITY(p) NVIC_SetPriority(TIM1_BRK_IRQn, (p))
#define NX_F1_TIM1_IRQ_ENABLE()    NVIC_EnableIRQ(TIM1_BRK_IRQn)
#define NX_F1_TIM1_IRQ_DISABLE()   NVIC_DisableIRQ(TIM1_BRK_IRQn)
#define NX_F1_TIM1_IRQ_CLEAR()     NVIC_ClearPendingIRQ(TIM1_BRK_IRQn)
#endif

enum {
    AFIO_OWNED = 1U,
    TIMER_OWNED = 2U,
    VECTOR_OWNED = 4U,
    READY = 8U,
};

static bool timer_clock(void) {
    return (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U;
}

static bool sampled_break(void) {
    return (GPIOB->IDR & BREAK_PIN) == 0U || (TIM1->SR & TIM_SR_BIF) != 0U;
}

/** \brief BKE, BKP and MOE readback follow an additional APB access. */
static uint32_t break_control(void) {
    nx_arch_dsb();
    (void)TIM1->BDTR;
    return TIM1->BDTR;
}

/** \brief Hold the independent GPIO latches low before removing the sources. */
static bool output_pins(nx_stm32f1_advanced_pwm_t* state, bool alternate) {
    bool main = !state->main_pads.captured ||
                ((GPIOA->ODR & MAIN_PIN) == 0U &&
                 nx_stm32f1_write_masked(&GPIOA->CRH, MAIN_FIELD,
                                         alternate ? MAIN_TIMER : MAIN_IDLE));
    bool comp = !state->other_pads.captured ||
                ((GPIOB->ODR & COMP_PIN) == 0U &&
                 nx_stm32f1_write_masked(&GPIOB->CRH, COMP_FIELD,
                                         alternate ? COMP_TIMER : COMP_IDLE));
    return main && comp;
}

/** \brief Attempt each independent path to withdraw outputs before retry. */
static bool park(nx_stm32f1_advanced_pwm_t* state) {
    bool clock = (state->effects & TIMER_OWNED) != 0U && timer_clock();
    bool disabled = true, detached = true, counter_stopped = true;
    bool source = true, vector = true;
    if (clock) {
        NX_STM32F1_WRITE(&TIM1->BDTR, TIM1->BDTR & ~(uint32_t)TIM_BDTR_MOE);
        disabled = (break_control() & TIM_BDTR_MOE) == 0U;
        source = nx_stm32f1_write_masked(&TIM1->DIER, TIM_DIER_BIE, 0U);
    }
    bool pins = output_pins(state, false);
    if (clock) {
        detached = nx_stm32f1_write_masked(&TIM1->CCER, OUTPUTS, 0U);
        counter_stopped = nx_stm32f1_write_masked(&TIM1->CR1, TIM_CR1_CEN, 0U);
        state->fault = state->fault || sampled_break();
    }
    if ((state->effects & VECTOR_OWNED) != 0U) {
        NX_F1_TIM1_IRQ_DISABLE();
        nx_arch_dsb();
        nx_arch_isb();
        if (!NX_F1_TIM1_IRQ_ACTIVE()) {
            NX_F1_TIM1_IRQ_CLEAR();
        }
        vector = !NX_F1_TIM1_IRQ_PENDING() && !NX_F1_TIM1_IRQ_ENABLED() &&
                 !NX_F1_TIM1_IRQ_ACTIVE();
    }
    return disabled && source && pins && detached && counter_stopped && vector;
}

static bool release_afio(nx_stm32f1_advanced_pwm_t* state) {
    if ((state->effects & AFIO_OWNED) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
            return false;
        }
        state->effects &= (uint8_t)~AFIO_OWNED;
    }
    return true;
}

static bool reset_timer(void) {
    const uint32_t bit = RCC_APB2RSTR_TIM1RST;
    return nx_stm32f1_write_masked(&RCC->APB2RSTR, bit, bit) &&
           nx_stm32f1_write_masked(&RCC->APB2RSTR, bit, 0U);
}

static bool hsi8_ready(void) {
    const uint32_t hsi = RCC_CR_HSION | RCC_CR_HSIRDY;
    return nx_stm32f1_system_live() && (RCC->CR & hsi) == hsi &&
           (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2)) == 0U;
}

static bool fixed_base(const nx_stm32f1_advanced_pwm_t* state) {
    const uint32_t protection = BASE_PROTECTION | state->config->deadtime_ticks;
    return TIM1->ARR == state->config->period_ticks - 1U && TIM1->PSC == 0U &&
           (TIM1->CR1 & ~(uint32_t)TIM_CR1_CEN) == TIM_CR1_ARPE &&
           TIM1->CR2 == 0U && TIM1->SMCR == 0U && TIM1->RCR == 0U &&
           TIM1->CCMR1 == PWM_MODE && TIM1->CCMR2 == 0U &&
           (break_control() & ~(uint32_t)TIM_BDTR_MOE) == protection;
}

nx_result_t nx_stm32f1_advanced_pwm_prepare(
    nx_stm32f1_advanced_pwm_t* state,
    const nx_stm32f1_advanced_pwm_config_t* config) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || config == NULL || config->tick_hz != TIMER_HZ ||
        config->period_ticks < 64U || config->period_ticks > UINT16_MAX ||
        config->deadtime_ticks == 0U || config->deadtime_ticks > 127U ||
        config->priority >= (1U << __NVIC_PRIO_BITS) || config->active_high ||
        config->idle_high || config->idle_complement_high) {
        return NX_ERROR_INVALID;
    }
    if (!hsi8_ready()) {
        return NX_ERROR_STATE;
    }
    if (state->config != NULL || timer_clock() ||
        (RCC->APB2RSTR & RCC_APB2RSTR_TIM1RST) != 0U ||
        NX_F1_TIM1_IRQ_ACTIVE() || NX_F1_TIM1_IRQ_ENABLED() ||
        NX_F1_TIM1_IRQ_PENDING()) {
        return NX_ERROR_BUSY;
    }
    state->config = config;
    if ((RCC->APB2ENR & RCC_APB2ENR_AFIOEN) == 0U) {
        state->effects |= AFIO_OWNED;
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN,
                                     RCC_APB2ENR_AFIOEN)) {
            return NX_ERROR_IO;
        }
    }
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_TIM1_REMAP) != 0U;
    if (!release_afio(state)) {
        return NX_ERROR_IO;
    }
    if (remapped) {
        state->config = NULL;
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result =
        nx_stm32f1_pads_capture(&state->main_pads, GPIOA, MAIN_PIN);
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_apply(&state->main_pads, 0U, MAIN_IDLE, 0U);
    }
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_capture(&state->other_pads, GPIOB,
                                         COMP_PIN | BREAK_PIN);
    }
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_apply(&state->other_pads, 0U,
                                       COMP_IDLE | BREAK_PULL, 0U);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    state->effects |= TIMER_OWNED;
    if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_TIM1EN,
                                 RCC_APB2ENR_TIM1EN) ||
        !reset_timer()) {
        return NX_ERROR_IO;
    }
    state->old_priority = (uint8_t)NX_F1_TIM1_IRQ_PRIORITY();
    state->effects |= VECTOR_OWNED;
    NX_F1_TIM1_SET_PRIORITY(config->priority);
    nx_arch_dsb();
    if (NX_F1_TIM1_IRQ_PRIORITY() != config->priority) {
        return NX_ERROR_IO;
    }
    LL_TIM_SetAutoReload(TIM1, config->period_ticks - 1U);
    LL_TIM_EnableARRPreload(TIM1);
    NX_STM32F1_WRITE(&TIM1->CCMR1, PWM_MODE);
    NX_STM32F1_WRITE(&TIM1->BDTR, BASE_PROTECTION | config->deadtime_ticks);
    if (!fixed_base(state) || TIM1->CCER != 0U || TIM1->DIER != 0U ||
        TIM1->CCR1 != 0U || (break_control() & TIM_BDTR_MOE) != 0U) {
        return NX_ERROR_IO;
    }
    state->fault = sampled_break();
    state->effects |= READY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_advanced_pwm_close(nx_stm32f1_advanced_pwm_t* state) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->effects &= (uint8_t)~READY;
    const bool parked = park(state);
    nx_arch_irq_restore(mask);
    if (!parked) {
        return NX_ERROR_IO;
    }
    if ((state->effects & VECTOR_OWNED) != 0U) {
        NX_F1_TIM1_SET_PRIORITY(state->old_priority);
        nx_arch_dsb();
        if (NX_F1_TIM1_IRQ_PRIORITY() != state->old_priority) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~VECTOR_OWNED;
    }
    if ((state->effects & TIMER_OWNED) != 0U) {
        if (!reset_timer() ||
            !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_TIM1EN, 0U)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~TIMER_OWNED;
    }
    if (!release_afio(state)) {
        return NX_ERROR_IO;
    }
    nx_result_t result = nx_stm32f1_pads_restore(&state->other_pads);
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_restore(&state->main_pads);
    }
    if (result == NX_SUCCESS) {
        *state = (nx_stm32f1_advanced_pwm_t){0};
    }
    return result;
}

/** \brief Update the compare shadow without adding an artificial period. */
static nx_result_t set_duty(void* context, uint32_t duty) {
    nx_stm32f1_advanced_pwm_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & READY) == 0U || !timer_clock() || state->fault ||
        sampled_break()) {
        return NX_ERROR_STATE;
    }
    if (duty > state->config->period_ticks) {
        return NX_ERROR_INVALID;
    }
    LL_TIM_OC_SetCompareCH1(TIM1, duty);
    nx_arch_dsb();
    if (LL_TIM_OC_GetCompareCH1(TIM1) != duty) {
        state->fault = true;
        (void)park(state);
        return NX_ERROR_IO;
    }
    state->duty = (uint16_t)duty;
    return NX_SUCCESS;
}

/** \brief Verify a fresh stopped update instead of reading write-only EGR. */
static bool load_preload(void) {
    NX_STM32F1_WRITE(&TIM1->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((TIM1->SR & TIM_SR_UIF) != 0U) {
        return false;
    }
    NX_STM32F1_WRITE(&TIM1->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    for (unsigned attempt = 0U; attempt < 16U; ++attempt) {
        if ((TIM1->SR & TIM_SR_UIF) != 0U) {
            return true;
        }
    }
    return false;
}

static nx_result_t start(void* context) {
    nx_stm32f1_advanced_pwm_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & READY) == 0U || !timer_clock()) {
        return NX_ERROR_STATE;
    }
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    state->fault = state->fault || sampled_break();
    if (!state->fault && hsi8_ready() && fixed_base(state) &&
        TIM1->CCR1 == state->duty) {
        if ((break_control() & TIM_BDTR_MOE) != 0U) {
            result = (TIM1->CCER == OUTPUTS && (TIM1->CR1 & TIM_CR1_CEN) != 0U)
                         ? NX_SUCCESS
                         : NX_ERROR_IO;
        } else if (TIM1->CCER == 0U && TIM1->DIER == 0U &&
                   (TIM1->CR1 & TIM_CR1_CEN) == 0U && load_preload() &&
                   nx_stm32f1_write_masked(&TIM1->CCER, OUTPUTS, OUTPUTS) &&
                   output_pins(state, true) &&
                   nx_stm32f1_write_masked(&TIM1->DIER, TIM_DIER_BIE,
                                           TIM_DIER_BIE) &&
                   nx_stm32f1_write_masked(&TIM1->CR1, TIM_CR1_CEN,
                                           TIM_CR1_CEN)) {
            NX_F1_TIM1_IRQ_ENABLE();
            nx_arch_dsb();
            if (NX_F1_TIM1_IRQ_ENABLED() && !sampled_break()) {
                NX_STM32F1_WRITE(&TIM1->BDTR, TIM1->BDTR | TIM_BDTR_MOE);
                const bool armed = (break_control() & TIM_BDTR_MOE) != 0U;
                state->fault = sampled_break();
                result = state->fault ? NX_ERROR_STATE
                         : armed      ? NX_SUCCESS
                                      : NX_ERROR_IO;
            }
        } else {
            result = NX_ERROR_IO;
        }
    }
    if (result != NX_SUCCESS) {
        state->fault = true;
        if (!park(state)) {
            result = NX_ERROR_IO;
        }
    }
    nx_arch_irq_restore(mask);
    return result;
}

static nx_result_t stop(void* context) {
    nx_stm32f1_advanced_pwm_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    const bool stopped = park(state);
    state->fault = state->fault || !stopped;
    nx_arch_irq_restore(mask);
    return stopped ? NX_SUCCESS : NX_ERROR_IO;
}

static nx_result_t snapshot(const void* context, nx_advanced_pwm_state_t* out) {
    const nx_stm32f1_advanced_pwm_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & READY) == 0U || !timer_clock()) {
        return NX_ERROR_STATE;
    }
    if (nx_io_dispatch_overlap(out, sizeof(*out), state, sizeof(*state)) ||
        nx_io_dispatch_overlap(out, sizeof(*out), state->config,
                               sizeof(*state->config))) {
        return NX_ERROR_INVALID;
    }
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    *out = (nx_advanced_pwm_state_t){
        .period_ticks = state->config->period_ticks,
        .duty_ticks = state->duty,
        .tick_hz = TIMER_HZ,
        .deadtime_ticks = state->config->deadtime_ticks,
        .deadtime_hz = TIMER_HZ,
        .running = (break_control() & TIM_BDTR_MOE) != 0U,
        .fault = state->fault || sampled_break(),
    };
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}

static nx_result_t clear_fault(void* context) {
    nx_stm32f1_advanced_pwm_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & READY) == 0U || !timer_clock()) {
        return NX_ERROR_STATE;
    }
    if ((break_control() & TIM_BDTR_MOE) != 0U ||
        (TIM1->CR1 & TIM_CR1_CEN) != 0U || TIM1->CCER != 0U ||
        TIM1->DIER != 0U || NX_F1_TIM1_IRQ_ENABLED() ||
        NX_F1_TIM1_IRQ_PENDING() || NX_F1_TIM1_IRQ_ACTIVE() ||
        (GPIOB->IDR & BREAK_PIN) == 0U) {
        return NX_ERROR_BUSY;
    }
    NX_STM32F1_WRITE(&TIM1->SR, ~(uint32_t)TIM_SR_BIF);
    nx_arch_dsb();
    state->fault = sampled_break();
    return state->fault ? NX_ERROR_BUSY : NX_SUCCESS;
}

void nx_stm32f1_advanced_pwm_irq(nx_stm32f1_advanced_pwm_t* state) {
    if ((state->effects & READY) != 0U && timer_clock() &&
        (TIM1->DIER & TIM_DIER_BIE) != 0U && (TIM1->SR & TIM_SR_BIF) != 0U) {
        state->fault = true;
        /* Keep BIF latched and mask the source even while BKIN stays active. */
        NX_STM32F1_WRITE(&TIM1->DIER, TIM1->DIER & ~(uint32_t)TIM_DIER_BIE);
        nx_arch_dsb();
    }
}

const nx_advanced_pwm_ops_t nx_stm32f1_advanced_pwm_ops = {
    set_duty, start, stop, snapshot, clear_fault};
