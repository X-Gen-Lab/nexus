/**
 * \file            pwm.c
 * \brief           STM32F103 TIM3 CH1 fixed-base PWM on PA6.
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

#define PA6_MASK  (UINT32_C(1) << 6U)
#define PA6_FIELD (GPIO_CRL_MODE6 | GPIO_CRL_CNF6)
#define PA6_IDLE  GPIO_CRL_MODE6_1
#define PA6_TIMER (GPIO_CRL_MODE6_1 | GPIO_CRL_CNF6_1)

enum {
    AFIO_OWNED = 1U,
    TIMER_OWNED = 2U,
    LIVE = 4U,
    RUNNING = 8U,
    FAULT = 16U
};

/** \brief Withdraw a temporary AFIO gate without changing the remap word. */
static bool release_afio(nx_stm32f1_pwm_t* state) {
    if ((state->effects & AFIO_OWNED) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
            return false;
        }
        state->effects &= (uint8_t)~AFIO_OWNED;
    }
    return true;
}

/** \brief Reset only the timer bank acquired by this canonical owner. */
static bool reset_timer(void) {
    const uint32_t mask = RCC_APB1RSTR_TIM3RST;
    return nx_stm32f1_write_masked(&RCC->APB1RSTR, mask, mask) &&
           nx_stm32f1_write_masked(&RCC->APB1RSTR, mask, 0U);
}

/** \brief Switch the owned pin between low GPIO and timer output. */
static bool connect_pin(bool alternate) {
    return nx_stm32f1_write_masked(&GPIOA->CRL, PA6_FIELD,
                                   alternate ? PA6_TIMER : PA6_IDLE);
}

/** \brief Park the wire first; a stopped timer could otherwise hold it high. */
static bool park_output(const nx_stm32f1_pwm_t* state) {
    if (state->pads.captured &&
        ((GPIOA->ODR & PA6_MASK) != 0U || !connect_pin(false))) {
        return false;
    }
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) == 0U) {
        return true;
    }
    return nx_stm32f1_write_masked(&TIM3->CCER, TIM_CCER_CC1E, 0U) &&
           nx_stm32f1_write_masked(&TIM3->CR1, TIM_CR1_CEN, 0U);
}

/** \brief Load the fixed timebase with output, IRQ and slave functions off. */
static bool configure_timer(const nx_stm32f1_pwm_config_t* config) {
    const uint32_t prescaler = config->clock_hz / config->tick_hz - 1U;
    const uint32_t period = config->period_ticks - 1U;
    const uint32_t compare_mode = LL_TIM_OCMODE_PWM1 | TIM_CCMR1_OC1PE;
    LL_TIM_SetPrescaler(TIM3, prescaler);
    LL_TIM_SetAutoReload(TIM3, period);
    LL_TIM_OC_SetCompareCH1(TIM3, config->duty_ticks);
    LL_TIM_EnableARRPreload(TIM3);
    NX_STM32F1_WRITE(&TIM3->CCMR1, compare_mode);
    nx_arch_dsb();
    return TIM3->PSC == prescaler && TIM3->ARR == period &&
           TIM3->CCR1 == config->duty_ticks && TIM3->CR1 == TIM_CR1_ARPE &&
           TIM3->CCMR1 == compare_mode && TIM3->CCER == 0U &&
           TIM3->SMCR == 0U && TIM3->DIER == 0U;
}

/** \brief Admit the fixed reset route and take the sole TIM3 owner. */
nx_result_t nx_stm32f1_pwm_prepare(nx_stm32f1_pwm_t* state,
                                   const nx_stm32f1_pwm_config_t* config) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || config == NULL || config->registers != TIM3 ||
        config->gpio != GPIOA || config->pin != 6U || config->channel != 1U ||
        config->inactive_latch != 0U || config->period_ticks <= 1U ||
        config->period_ticks > UINT16_MAX ||
        config->duty_ticks > config->period_ticks || config->tick_hz == 0U ||
        config->clock_hz % config->tick_hz != 0U ||
        config->clock_hz / config->tick_hz > UINT32_C(65536)) {
        return NX_ERROR_INVALID;
    }
    if (config->clock_hz != 8000000U || !nx_stm32f1_system_live() ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) !=
            RCC_CFGR_SWS_HSI) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->config != NULL || (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_TIM3RST) != 0U) {
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
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_TIM3_REMAP) != 0U;
    if (!release_afio(state)) {
        return NX_ERROR_IO;
    }
    if (remapped) {
        state->config = NULL;
        return NX_ERROR_UNSUPPORTED;
    }

    nx_result_t result = nx_stm32f1_pads_capture(&state->pads, GPIOA, PA6_MASK);
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_apply(&state->pads, PA6_IDLE, 0U, 0U);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    state->effects |= TIMER_OWNED;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM3EN,
                                 RCC_APB1ENR_TIM3EN) ||
        !reset_timer() || !configure_timer(config)) {
        return NX_ERROR_IO;
    }
    state->duty = (uint16_t)config->duty_ticks;
    state->effects |= LIVE;
    return NX_SUCCESS;
}

/** \brief Withdraw the output and return only the bank and pad this mode owns.
 */
nx_result_t nx_stm32f1_pwm_close(nx_stm32f1_pwm_t* state) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & TIMER_OWNED) != 0U) {
        if (!park_output(state) || !reset_timer() ||
            !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM3EN, 0U)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t) ~(TIMER_OWNED | RUNNING);
    }
    if (!release_afio(state)) {
        return NX_ERROR_IO;
    }
    const nx_result_t result = nx_stm32f1_pads_restore(&state->pads);
    if (result == NX_SUCCESS) {
        state->config = NULL;
        state->duty = 0U;
        state->effects = 0U;
    }
    return result;
}

/** \brief Stage one compare preload for the next natural timer update. */
static nx_result_t set(void* context, uint32_t period_ticks,
                       uint32_t duty_ticks) {
    nx_stm32f1_pwm_t* state = context;
    if ((state->effects & (LIVE | FAULT)) != LIVE) {
        return NX_ERROR_STATE;
    }
    if (period_ticks != state->config->period_ticks ||
        duty_ticks > period_ticks) {
        return NX_ERROR_INVALID;
    }
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U) {
        LL_TIM_OC_SetCompareCH1(TIM3, duty_ticks);
        nx_arch_dsb();
        if (LL_TIM_OC_GetCompareCH1(TIM3) == duty_ticks) {
            state->duty = (uint16_t)duty_ticks;
            return NX_SUCCESS;
        }
    }
    state->effects |= FAULT;
    return NX_ERROR_IO;
}

/** \brief Acknowledge a fresh preload update before connecting the output. */
static bool load_preloads(void) {
    LL_TIM_ClearFlag_UPDATE(TIM3);
    nx_arch_dsb();
    if (LL_TIM_IsActiveFlag_UPDATE(TIM3) != 0U) {
        return false;
    }
    /* EGR is write-only. The reviewed APB1/timer clock is HSI8. */
    NX_STM32F1_WRITE(&TIM3->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    for (unsigned samples = 0U; samples < 16U; ++samples) {
        if (LL_TIM_IsActiveFlag_UPDATE(TIM3) != 0U) {
            LL_TIM_ClearFlag_UPDATE(TIM3);
            return true;
        }
    }
    return false;
}

/** \brief Attach the alternate output at the inactive end of the first period.
 */
static nx_result_t start(void* context) {
    nx_stm32f1_pwm_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & (LIVE | FAULT)) != LIVE) {
        return NX_ERROR_STATE;
    }
    if ((state->effects & RUNNING) != 0U) {
        return NX_SUCCESS;
    }
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U && load_preloads()) {
        const uint32_t end = state->config->period_ticks - 1U;
        LL_TIM_SetCounter(TIM3, end);
        nx_arch_dsb();
        if (LL_TIM_GetCounter(TIM3) == end &&
            nx_stm32f1_write_masked(&TIM3->CCER, TIM_CCER_CC1E,
                                    TIM_CCER_CC1E) &&
            connect_pin(true) &&
            nx_stm32f1_write_masked(&TIM3->CR1, TIM_CR1_CEN, TIM_CR1_CEN)) {
            state->effects |= RUNNING;
            return NX_SUCCESS;
        }
    }
    state->effects |= FAULT;
    (void)park_output(state);
    return NX_ERROR_IO;
}

/** \brief Stop only after verifying the pin's inactive level ownership. */
static nx_result_t stop(void* context) {
    nx_stm32f1_pwm_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & LIVE) == 0U) {
        return NX_ERROR_STATE;
    }
    if (!park_output(state)) {
        state->effects |= FAULT;
        return NX_ERROR_IO;
    }
    state->effects &= (uint8_t)~RUNNING;
    return NX_SUCCESS;
}

/** \brief Return the last admitted staged counts while the view is live. */
static nx_result_t get_state(const void* context, nx_pwm_state_t* output) {
    const nx_stm32f1_pwm_t* state = context;
    if ((state->effects & (LIVE | FAULT)) != LIVE) {
        return NX_ERROR_STATE;
    }
    if (!nx_io_dispatch_span(output, sizeof(*output),
                             _Alignof(nx_pwm_state_t)) ||
        nx_io_dispatch_overlap(output, sizeof(*output), state,
                               sizeof(*state)) ||
        nx_io_dispatch_overlap(output, sizeof(*output), state->config,
                               sizeof(*state->config))) {
        return NX_ERROR_INVALID;
    }
    *output = (nx_pwm_state_t){
        .period_ticks = state->config->period_ticks,
        .duty_ticks = state->duty,
        .tick_hz = state->config->tick_hz,
        .running = (state->effects & RUNNING) != 0U,
    };
    return NX_SUCCESS;
}

const nx_pwm_ops_t nx_stm32f1_pwm_ops = {
    .set = set,
    .start = start,
    .stop = stop,
    .state = get_state,
};
