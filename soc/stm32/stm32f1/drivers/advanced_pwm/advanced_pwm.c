/**
 * \file            advanced_pwm.c
 * \brief           STM32F103 TIM1/TIM8 safe complementary output and break.
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

/* Each exact assembly selects one fixed timer and pad group. */
#if defined(STM32F103xB)
#define APWM_TIMER TIM1
#define APWM_GATE  RCC_APB2ENR_TIM1EN
#define APWM_RESET RCC_APB2RSTR_TIM1RST
#define APWM_IRQ   TIM1_BRK_IRQn
#define MAIN_PORT  GPIOA
#define OTHER_PORT GPIOB
#define MAIN_LINE  8U
#define COMP_LINE  13U
#define BREAK_LINE 12U
#elif defined(STM32F103xE)
#define APWM_TIMER TIM8
#define APWM_GATE  RCC_APB2ENR_TIM8EN
#define APWM_RESET RCC_APB2RSTR_TIM8RST
#define APWM_IRQ   TIM8_BRK_IRQn
#define MAIN_PORT  GPIOC
#define OTHER_PORT GPIOA
#define MAIN_LINE  6U
#define COMP_LINE  7U
#define BREAK_LINE 6U
#else
#error "F1 advanced PWM requires the reviewed F103RB or F103ZE timer"
#endif

#define PIN(line)                 (UINT32_C(1) << (line))
#define MODE_SHIFT(line)          (4U * ((line) % 8U))
#define GPIO_MODE(line, mode)     ((uint32_t)(mode) << MODE_SHIFT(line))
#define MODE_REGISTER(port, line) ((line) < 8U ? &(port)->CRL : &(port)->CRH)
#if defined(NX_STM32F1_CYCLE_THREE_LEGS)
#if !defined(STM32F103xB) || !defined(NX_STM32F1_PWM_ADC_OWNER)
#error "Three output pairs require the F103RB finite TIM1/ADC1 owner"
#endif
#define LEGS(value, stride)                                                    \
    ((value) | ((value) << (stride)) | ((value) << (2U * (stride))))
#else
#define LEGS(value, stride) (value)
#endif
#define MAIN_PIN        LEGS(PIN(MAIN_LINE), 1U)
#define COMP_PIN        LEGS(PIN(COMP_LINE), 1U)
#define BREAK_PIN       PIN(BREAK_LINE)
#define BREAK_PULL      GPIO_MODE(BREAK_LINE, GPIO_CRL_CNF0_1)
#define OUTPUT_FIELD    (GPIO_CRL_MODE0 | GPIO_CRL_CNF0)
#define OUTPUT_IDLE     GPIO_CRL_MODE0_1
#define OUTPUT_TIMER    (GPIO_CRL_MODE0_1 | GPIO_CRL_CNF0_1)
#define MAIN_FIELD      LEGS(GPIO_MODE(MAIN_LINE, OUTPUT_FIELD), 4U)
#define COMP_FIELD      LEGS(GPIO_MODE(COMP_LINE, OUTPUT_FIELD), 4U)
#define MAIN_IDLE       LEGS(GPIO_MODE(MAIN_LINE, OUTPUT_IDLE), 4U)
#define COMP_IDLE       LEGS(GPIO_MODE(COMP_LINE, OUTPUT_IDLE), 4U)
#define MAIN_TIMER      LEGS(GPIO_MODE(MAIN_LINE, OUTPUT_TIMER), 4U)
#define COMP_TIMER      LEGS(GPIO_MODE(COMP_LINE, OUTPUT_TIMER), 4U)
#define TIMER_HZ        UINT32_C(8000000)
#define OUTPUTS         LEGS((TIM_CCER_CC1E | TIM_CCER_CC1NE), 4U)
#define BASE_PROTECTION (TIM_BDTR_BKE | TIM_BDTR_OSSI | TIM_BDTR_OSSR)
#define PWM_MODE        (LL_TIM_OCMODE_PWM1 | TIM_CCMR1_OC1PE)
#if defined(NX_STM32F1_CYCLE_THREE_LEGS)
#define PWM_CCMR1 (PWM_MODE | (PWM_MODE << 8U))
#define PWM_CCMR2 PWM_MODE
#else
#define PWM_CCMR1 PWM_MODE
#define PWM_CCMR2 0U
#endif

_Static_assert((COMP_LINE < 8U) == (BREAK_LINE < 8U),
               "The complementary output and break share one pad register");

#ifndef NX_F1_APWM_IRQ_ENABLED
#define NX_F1_APWM_IRQ_ENABLED(irq)     NVIC_GetEnableIRQ(irq)
#define NX_F1_APWM_IRQ_PENDING(irq)     NVIC_GetPendingIRQ(irq)
#define NX_F1_APWM_IRQ_ACTIVE(irq)      NVIC_GetActive(irq)
#define NX_F1_APWM_IRQ_PRIORITY(irq)    NVIC_GetPriority(irq)
#define NX_F1_APWM_SET_PRIORITY(irq, p) NVIC_SetPriority((irq), (p))
#define NX_F1_APWM_IRQ_ENABLE(irq)      NVIC_EnableIRQ(irq)
#define NX_F1_APWM_IRQ_DISABLE(irq)     NVIC_DisableIRQ(irq)
#define NX_F1_APWM_IRQ_CLEAR(irq)       NVIC_ClearPendingIRQ(irq)
#endif

enum {
    AFIO_OWNED = 1U,
    TIMER_OWNED = 2U,
    VECTOR_OWNED = 4U,
    READY = 8U,
};

static bool timer_clock(void) {
    return (RCC->APB2ENR & APWM_GATE) != 0U;
}

static bool sampled_break(void) {
    return (OTHER_PORT->IDR & BREAK_PIN) == 0U ||
           (APWM_TIMER->SR & TIM_SR_BIF) != 0U;
}

/** \brief BKE, BKP and MOE readback follow an additional APB access. */
static uint32_t break_control(void) {
    nx_arch_dsb();
    (void)APWM_TIMER->BDTR;
    return APWM_TIMER->BDTR;
}

/** \brief Hold the independent GPIO latches low before removing the sources. */
static bool output_pins(nx_stm32f1_advanced_pwm_t* state, bool alternate) {
    bool main = !state->main_pads.captured ||
                ((MAIN_PORT->ODR & MAIN_PIN) == 0U &&
                 nx_stm32f1_write_masked(MODE_REGISTER(MAIN_PORT, MAIN_LINE),
                                         MAIN_FIELD,
                                         alternate ? MAIN_TIMER : MAIN_IDLE));
    bool comp = !state->other_pads.captured ||
                ((OTHER_PORT->ODR & COMP_PIN) == 0U &&
                 nx_stm32f1_write_masked(MODE_REGISTER(OTHER_PORT, COMP_LINE),
                                         COMP_FIELD,
                                         alternate ? COMP_TIMER : COMP_IDLE));
    return main && comp;
}

/** \brief Attempt each independent path to withdraw outputs before retry. */
static bool park(nx_stm32f1_advanced_pwm_t* state) {
    bool clock = (state->effects & TIMER_OWNED) != 0U && timer_clock();
    bool disabled = true, detached = true, counter_stopped = true;
    bool source = true, vector = true;
    if (clock) {
        NX_STM32F1_WRITE(&APWM_TIMER->BDTR,
                         APWM_TIMER->BDTR & ~(uint32_t)TIM_BDTR_MOE);
        disabled = (break_control() & TIM_BDTR_MOE) == 0U;
        source = nx_stm32f1_write_masked(&APWM_TIMER->DIER, TIM_DIER_BIE, 0U);
    }
    bool pins = output_pins(state, false);
    if (clock) {
        detached = nx_stm32f1_write_masked(&APWM_TIMER->CCER, OUTPUTS, 0U);
        counter_stopped =
            nx_stm32f1_write_masked(&APWM_TIMER->CR1, TIM_CR1_CEN, 0U);
        state->fault = state->fault || sampled_break();
    }
    if ((state->effects & VECTOR_OWNED) != 0U) {
        NX_F1_APWM_IRQ_DISABLE(APWM_IRQ);
        nx_arch_dsb();
        nx_arch_isb();
        if (!NX_F1_APWM_IRQ_ACTIVE(APWM_IRQ)) {
            NX_F1_APWM_IRQ_CLEAR(APWM_IRQ);
        }
        vector = !NX_F1_APWM_IRQ_PENDING(APWM_IRQ) &&
                 !NX_F1_APWM_IRQ_ENABLED(APWM_IRQ) &&
                 !NX_F1_APWM_IRQ_ACTIVE(APWM_IRQ);
    }
    return disabled && source && pins && detached && counter_stopped && vector;
}

static bool release_afio(nx_stm32f1_advanced_pwm_t* state) {
#if defined(STM32F103xB)
    if ((state->effects & AFIO_OWNED) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
            return false;
        }
        state->effects &= (uint8_t)~AFIO_OWNED;
    }
#else
    (void)state;
#endif
    return true;
}

static bool reset_timer(void) {
    const uint32_t bit = APWM_RESET;
    return nx_stm32f1_write_masked(&RCC->APB2RSTR, bit, bit) &&
           nx_stm32f1_write_masked(&RCC->APB2RSTR, bit, 0U);
}

static bool hsi8_ready(void) {
    const uint32_t hsi = RCC_CR_HSION | RCC_CR_HSIRDY;
    return nx_stm32f1_system_live() && (RCC->CR & hsi) == hsi &&
           (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2)) == 0U;
}

static bool fixed_base(const nx_stm32f1_advanced_pwm_t* state, uint32_t mode) {
    const uint32_t protection = BASE_PROTECTION | state->config->deadtime_ticks;
    return APWM_TIMER->ARR == state->config->period_ticks - 1U &&
           APWM_TIMER->PSC == 0U &&
           (APWM_TIMER->CR1 & ~(uint32_t)TIM_CR1_CEN) == mode &&
           APWM_TIMER->CR2 == 0U && APWM_TIMER->SMCR == 0U &&
           APWM_TIMER->RCR == 0U && APWM_TIMER->CCMR1 == PWM_CCMR1 &&
           APWM_TIMER->CCMR2 == PWM_CCMR2 &&
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
        (RCC->APB2RSTR & APWM_RESET) != 0U || NX_F1_APWM_IRQ_ACTIVE(APWM_IRQ) ||
        NX_F1_APWM_IRQ_ENABLED(APWM_IRQ) || NX_F1_APWM_IRQ_PENDING(APWM_IRQ)) {
        return NX_ERROR_BUSY;
    }
    state->config = config;
#if defined(STM32F103xB)
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
#endif
    const uint32_t other_modes = COMP_IDLE | BREAK_PULL;
    nx_result_t result =
        nx_stm32f1_pads_capture(&state->main_pads, MAIN_PORT, MAIN_PIN);
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_apply(&state->main_pads,
                                       MAIN_LINE < 8U ? MAIN_IDLE : 0U,
                                       MAIN_LINE < 8U ? 0U : MAIN_IDLE, 0U);
    }
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_capture(&state->other_pads, OTHER_PORT,
                                         COMP_PIN | BREAK_PIN);
    }
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_apply(&state->other_pads,
                                       COMP_LINE < 8U ? other_modes : 0U,
                                       COMP_LINE < 8U ? 0U : other_modes, 0U);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    state->effects |= TIMER_OWNED;
    if (!nx_stm32f1_write_masked(&RCC->APB2ENR, APWM_GATE, APWM_GATE) ||
        !reset_timer()) {
        return NX_ERROR_IO;
    }
    state->old_priority = (uint8_t)NX_F1_APWM_IRQ_PRIORITY(APWM_IRQ);
    state->effects |= VECTOR_OWNED;
    NX_F1_APWM_SET_PRIORITY(APWM_IRQ, config->priority);
    nx_arch_dsb();
    if (NX_F1_APWM_IRQ_PRIORITY(APWM_IRQ) != config->priority) {
        return NX_ERROR_IO;
    }
    LL_TIM_SetAutoReload(APWM_TIMER, config->period_ticks - 1U);
    LL_TIM_EnableARRPreload(APWM_TIMER);
    NX_STM32F1_WRITE(&APWM_TIMER->CCMR1, PWM_CCMR1);
#if defined(NX_STM32F1_CYCLE_THREE_LEGS)
    NX_STM32F1_WRITE(&APWM_TIMER->CCMR2, PWM_CCMR2);
#endif
    NX_STM32F1_WRITE(&APWM_TIMER->BDTR,
                     BASE_PROTECTION | config->deadtime_ticks);
    if (!fixed_base(state, TIM_CR1_ARPE) || APWM_TIMER->CCER != 0U ||
        APWM_TIMER->DIER != 0U || APWM_TIMER->CCR1 != 0U ||
        (break_control() & TIM_BDTR_MOE) != 0U) {
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
        NX_F1_APWM_SET_PRIORITY(APWM_IRQ, state->old_priority);
        nx_arch_dsb();
        if (NX_F1_APWM_IRQ_PRIORITY(APWM_IRQ) != state->old_priority) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~VECTOR_OWNED;
    }
    if ((state->effects & TIMER_OWNED) != 0U) {
        if (!reset_timer() ||
            !nx_stm32f1_write_masked(&RCC->APB2ENR, APWM_GATE, 0U)) {
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
    LL_TIM_OC_SetCompareCH1(APWM_TIMER, duty);
    nx_arch_dsb();
    if (LL_TIM_OC_GetCompareCH1(APWM_TIMER) != duty) {
        state->fault = true;
        (void)park(state);
        return NX_ERROR_IO;
    }
    state->duty = (uint16_t)duty;
    return NX_SUCCESS;
}

/** \brief Verify a fresh stopped update instead of reading write-only EGR. */
static bool load_preload(void) {
    NX_STM32F1_WRITE(&APWM_TIMER->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((APWM_TIMER->SR & TIM_SR_UIF) != 0U) {
        return false;
    }
    NX_STM32F1_WRITE(&APWM_TIMER->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    for (unsigned attempt = 0U; attempt < 16U; ++attempt) {
        if ((APWM_TIMER->SR & TIM_SR_UIF) != 0U) {
            return true;
        }
    }
    return false;
}

static bool clear_window_events(uint32_t sources) {
#if defined(NX_STM32F1_PWM_ADC_OWNER)
    if ((sources & TIM_DIER_UIE) != 0U) {
        const uint32_t flags = TIM_SR_UIF | TIM_SR_CC4IF;
        NX_STM32F1_WRITE(&APWM_TIMER->SR, ~flags);
        nx_arch_dsb();
        return (APWM_TIMER->SR & flags) == 0U;
    }
#else
    (void)sources;
#endif
    return true;
}

static nx_result_t start_mode(void* context, uint32_t mode, uint32_t sources) {
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
    if (!state->fault && hsi8_ready() && fixed_base(state, mode) &&
        APWM_TIMER->CCR1 == state->duty) {
        if ((break_control() & TIM_BDTR_MOE) != 0U) {
            result = (APWM_TIMER->CCER == OUTPUTS &&
                      (APWM_TIMER->CR1 & TIM_CR1_CEN) != 0U)
                         ? NX_SUCCESS
                         : NX_ERROR_IO;
        } else if (APWM_TIMER->CCER == 0U && APWM_TIMER->DIER == 0U &&
                   (APWM_TIMER->CR1 & TIM_CR1_CEN) == 0U && load_preload() &&
                   clear_window_events(sources) &&
                   nx_stm32f1_write_masked(&APWM_TIMER->CCER, OUTPUTS,
                                           OUTPUTS) &&
                   output_pins(state, true) &&
                   nx_stm32f1_write_masked(&APWM_TIMER->DIER, sources,
                                           sources) &&
                   nx_stm32f1_write_masked(&APWM_TIMER->CR1, TIM_CR1_CEN,
                                           TIM_CR1_CEN)) {
            NX_F1_APWM_IRQ_ENABLE(APWM_IRQ);
            nx_arch_dsb();
            if (NX_F1_APWM_IRQ_ENABLED(APWM_IRQ) && !sampled_break()) {
                NX_STM32F1_WRITE(&APWM_TIMER->BDTR,
                                 APWM_TIMER->BDTR | TIM_BDTR_MOE);
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

static nx_result_t start(void* context) {
    return start_mode(context, TIM_CR1_ARPE, TIM_DIER_BIE);
}

#if defined(NX_STM32F1_PWM_ADC_OWNER)
nx_result_t nx_stm32f1_advanced_pwm_one_pulse(nx_stm32f1_advanced_pwm_t* state,
                                              const uint32_t* duties,
                                              uint32_t phase) {
    nx_result_t result = set_duty(state, duties[0]);
    if (result != NX_SUCCESS)
        return result;
#if defined(NX_STM32F1_CYCLE_THREE_LEGS)
    /* Load one complete stopped frame before the single common update. */
    NX_STM32F1_WRITE(&APWM_TIMER->CCR2, duties[1]);
    NX_STM32F1_WRITE(&APWM_TIMER->CCR3, duties[2]);
    nx_arch_dsb();
    if (APWM_TIMER->CCR2 != duties[1] || APWM_TIMER->CCR3 != duties[2])
        return NX_ERROR_IO;
#endif
    const uint32_t mode = TIM_CR1_ARPE | TIM_CR1_OPM;
    NX_STM32F1_WRITE(&APWM_TIMER->CCR4, phase);
    nx_arch_dsb();
    if (APWM_TIMER->CCR4 != phase ||
        !nx_stm32f1_write_masked(&APWM_TIMER->CR1, TIM_CR1_OPM, TIM_CR1_OPM))
        return NX_ERROR_IO;
    return start_mode(state, mode, TIM_DIER_BIE | TIM_DIER_UIE);
}
#endif

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
        (APWM_TIMER->CR1 & TIM_CR1_CEN) != 0U || APWM_TIMER->CCER != 0U ||
        APWM_TIMER->DIER != 0U || NX_F1_APWM_IRQ_ENABLED(APWM_IRQ) ||
        NX_F1_APWM_IRQ_PENDING(APWM_IRQ) || NX_F1_APWM_IRQ_ACTIVE(APWM_IRQ) ||
        (OTHER_PORT->IDR & BREAK_PIN) == 0U) {
        return NX_ERROR_BUSY;
    }
    NX_STM32F1_WRITE(&APWM_TIMER->SR, ~(uint32_t)TIM_SR_BIF);
    nx_arch_dsb();
    state->fault = sampled_break();
    return state->fault ? NX_ERROR_BUSY : NX_SUCCESS;
}

void nx_stm32f1_advanced_pwm_irq(nx_stm32f1_advanced_pwm_t* state) {
    if ((state->effects & READY) != 0U && timer_clock() &&
        (APWM_TIMER->DIER & TIM_DIER_BIE) != 0U &&
        (APWM_TIMER->SR & TIM_SR_BIF) != 0U) {
        state->fault = true;
        /* Keep BIF latched and mask the source even while BKIN stays active. */
        NX_STM32F1_WRITE(&APWM_TIMER->DIER,
                         APWM_TIMER->DIER & ~(uint32_t)TIM_DIER_BIE);
        nx_arch_dsb();
    }
}

const nx_advanced_pwm_ops_t nx_stm32f1_advanced_pwm_ops = {
    set_duty, start, stop, snapshot, clear_fault};
