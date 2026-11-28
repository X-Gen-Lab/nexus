/**
 * \file            pwm.c
 * \brief           STM32F1 fixed-base single-channel PWM.
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

typedef struct {
    TIM_TypeDef* timer;
    GPIO_TypeDef* gpio;
    uint32_t remap;
    uint8_t pin;
    uint8_t gate_shift;
} pwm_route_t;

#if defined(STM32F100xB)
#define PWM_GATE_REGISTER  (&RCC->APB2ENR)
#define PWM_RESET_REGISTER (&RCC->APB2RSTR)
#define PWM_REMAP_REGISTER (&AFIO->MAPR2)
#define PWM_BUS_DIVIDER    RCC_CFGR_PPRE2
static const pwm_route_t routes[] = {
    {TIM15, GPIOA, AFIO_MAPR2_TIM15_REMAP, 2U, RCC_APB2ENR_TIM15EN_Pos},
    {TIM16, GPIOB, AFIO_MAPR2_TIM16_REMAP, 8U, RCC_APB2ENR_TIM16EN_Pos},
    {TIM17, GPIOB, AFIO_MAPR2_TIM17_REMAP, 9U, RCC_APB2ENR_TIM17EN_Pos},
};
_Static_assert(RCC_APB2ENR_TIM15EN_Pos == RCC_APB2RSTR_TIM15RST_Pos &&
                   RCC_APB2ENR_TIM16EN_Pos == RCC_APB2RSTR_TIM16RST_Pos &&
                   RCC_APB2ENR_TIM17EN_Pos == RCC_APB2RSTR_TIM17RST_Pos,
               "Value Line PWM reset and enable bits must match");
#else
#define PWM_GATE_REGISTER  (&RCC->APB1ENR)
#define PWM_RESET_REGISTER (&RCC->APB1RSTR)
#define PWM_REMAP_REGISTER (&AFIO->MAPR)
#define PWM_BUS_DIVIDER    RCC_CFGR_PPRE1
static const pwm_route_t routes[] = {
    {TIM3, GPIOA, AFIO_MAPR_TIM3_REMAP, 6U, RCC_APB1ENR_TIM3EN_Pos},
};
_Static_assert(RCC_APB1ENR_TIM3EN_Pos == RCC_APB1RSTR_TIM3RST_Pos,
               "TIM3 reset and enable bits must match");
#endif

static const pwm_route_t*
reviewed_route(const nx_stm32f1_pwm_config_t* config) {
    if (config != NULL) {
        for (size_t i = 0U; i < sizeof(routes) / sizeof(routes[0]); ++i) {
            const pwm_route_t* route = &routes[i];
            if (config->registers == route->timer &&
                config->gpio == route->gpio && config->pin == route->pin) {
                return route;
            }
        }
    }
    return NULL;
}

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

/** \brief Reset only the selected timer and verify both clock transitions. */
static bool reset_timer(uint32_t mask) {
    return nx_stm32f1_write_masked(PWM_RESET_REGISTER, mask, mask) &&
           nx_stm32f1_write_masked(PWM_RESET_REGISTER, mask, 0U);
}

static uint32_t gpio_mode(uint8_t pin, bool alternate) {
    return (alternate ? 0xAU : 0x2U) << (4U * ((uint32_t)pin % 8U));
}

/** \brief Switch one preloaded low latch between GPIO and the timer. */
static bool connect_pin(const nx_stm32f1_pwm_config_t* cfg, bool alternate) {
    volatile uint32_t* reg = cfg->pin < 8U ? &cfg->gpio->CRL : &cfg->gpio->CRH;
    const uint32_t mask = 0xFU << (4U * ((uint32_t)cfg->pin % 8U));
    return nx_stm32f1_write_masked(reg, mask, gpio_mode(cfg->pin, alternate));
}

/** \brief Value Line timers gate the output globally through BDTR.MOE. */
static bool master_output(TIM_TypeDef* timer, bool enabled) {
#if defined(STM32F100xB)
    const uint32_t value = enabled ? TIM_BDTR_MOE : 0U;
    NX_STM32F1_WRITE(&timer->BDTR, value);
    nx_arch_dsb();
    (void)timer->BDTR;
    return timer->BDTR == value;
#else
    (void)timer;
    (void)enabled;
    return true;
#endif
}

/** \brief Park the wire first; a stopped counter could hold an active level. */
static bool park_output(const nx_stm32f1_pwm_t* state) {
    const nx_stm32f1_pwm_config_t* config = state->config;
    if (state->pads.captured &&
        ((config->gpio->ODR & (1U << config->pin)) != 0U ||
         !connect_pin(config, false))) {
#if defined(STM32F100xB)
        if ((*PWM_GATE_REGISTER & (1U << state->gate_shift)) != 0U) {
            (void)master_output(config->registers, false);
        }
#endif
        return false;
    }
    if ((*PWM_GATE_REGISTER & (1U << state->gate_shift)) == 0U) {
        return true;
    }
    TIM_TypeDef* timer = config->registers;
    return master_output(timer, false) &&
           nx_stm32f1_write_masked(&timer->CCER, TIM_CCER_CC1E, 0U) &&
           nx_stm32f1_write_masked(&timer->CR1, TIM_CR1_CEN, 0U);
}

/** \brief Load the fixed timebase with output, IRQ and slave functions off. */
static bool configure_timer(const nx_stm32f1_pwm_config_t* config) {
    TIM_TypeDef* timer = config->registers;
    const uint32_t prescaler = config->clock_hz / config->tick_hz - 1U;
    const uint32_t period = config->period_ticks - 1U;
    const uint32_t compare_mode = LL_TIM_OCMODE_PWM1 | TIM_CCMR1_OC1PE;
    LL_TIM_SetPrescaler(timer, prescaler);
    LL_TIM_SetAutoReload(timer, period);
    LL_TIM_OC_SetCompareCH1(timer, config->duty_ticks);
    LL_TIM_EnableARRPreload(timer);
    NX_STM32F1_WRITE(&timer->CCMR1, compare_mode);
    nx_arch_dsb();
    return timer->PSC == prescaler && timer->ARR == period &&
           timer->CCR1 == config->duty_ticks && timer->CR1 == TIM_CR1_ARPE &&
           timer->CCMR1 == compare_mode && timer->CCER == 0U &&
           timer->DIER == 0U &&
#if defined(STM32F100xB)
           timer->BDTR == 0U && timer->RCR == 0U &&
           (timer != TIM15 || timer->SMCR == 0U);
#else
           timer->SMCR == 0U;
#endif
}

/** \brief Admit the fixed reset route and take the selected timer owner. */
nx_result_t nx_stm32f1_pwm_prepare(nx_stm32f1_pwm_t* state,
                                   const nx_stm32f1_pwm_config_t* config) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    const pwm_route_t* route = reviewed_route(config);
    if (state == NULL || route == NULL || config->channel != 1U ||
        config->inactive_latch != 0U || config->period_ticks <= 1U ||
        config->period_ticks > UINT16_MAX ||
        config->duty_ticks > config->period_ticks || config->tick_hz == 0U ||
        config->clock_hz % config->tick_hz != 0U ||
        config->clock_hz / config->tick_hz > UINT32_C(65536)) {
        return NX_ERROR_INVALID;
    }
    if (config->clock_hz != 8000000U || !nx_stm32f1_system_live() ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | PWM_BUS_DIVIDER)) !=
            RCC_CFGR_SWS_HSI) {
        return NX_ERROR_UNSUPPORTED;
    }
    const uint32_t gate = 1U << route->gate_shift;
    if (state->config != NULL || (*PWM_GATE_REGISTER & gate) != 0U ||
        (*PWM_RESET_REGISTER & gate) != 0U) {
        return NX_ERROR_BUSY;
    }
    state->config = config;
    state->gate_shift = route->gate_shift;
    if ((RCC->APB2ENR & RCC_APB2ENR_AFIOEN) == 0U) {
        state->effects |= AFIO_OWNED;
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN,
                                     RCC_APB2ENR_AFIOEN)) {
            return NX_ERROR_IO;
        }
    }
    const bool remapped = (*PWM_REMAP_REGISTER & route->remap) != 0U;
    if (!release_afio(state)) {
        return NX_ERROR_IO;
    }
    if (remapped) {
        state->config = NULL;
        return NX_ERROR_UNSUPPORTED;
    }

    const uint16_t pin_mask = (uint16_t)(1U << route->pin);
    nx_result_t result =
        nx_stm32f1_pads_capture(&state->pads, route->gpio, pin_mask);
    if (result == NX_SUCCESS) {
        const uint32_t idle = gpio_mode(route->pin, false);
        result =
            nx_stm32f1_pads_apply(&state->pads, route->pin < 8U ? idle : 0U,
                                  route->pin < 8U ? 0U : idle, 0U);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    state->effects |= TIMER_OWNED;
    if (!nx_stm32f1_write_masked(PWM_GATE_REGISTER, gate, gate) ||
        !reset_timer(gate) || !configure_timer(config)) {
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
        const uint32_t gate = 1U << state->gate_shift;
        if (!park_output(state) || !reset_timer(gate) ||
            !nx_stm32f1_write_masked(PWM_GATE_REGISTER, gate, 0U)) {
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
        state->gate_shift = 0U;
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
    if ((*PWM_GATE_REGISTER & (1U << state->gate_shift)) != 0U) {
        TIM_TypeDef* timer = state->config->registers;
        LL_TIM_OC_SetCompareCH1(timer, duty_ticks);
        nx_arch_dsb();
        if (LL_TIM_OC_GetCompareCH1(timer) == duty_ticks) {
            state->duty = (uint16_t)duty_ticks;
            return NX_SUCCESS;
        }
    }
    state->effects |= FAULT;
    return NX_ERROR_IO;
}

/** \brief Acknowledge a fresh preload update before connecting the output. */
static bool load_preloads(TIM_TypeDef* timer) {
    LL_TIM_ClearFlag_UPDATE(timer);
    nx_arch_dsb();
    if (LL_TIM_IsActiveFlag_UPDATE(timer) != 0U) {
        return false;
    }
    /* EGR is write-only. The reviewed APB/timer clock is HSI8. */
    NX_STM32F1_WRITE(&timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    for (unsigned samples = 0U; samples < 16U; ++samples) {
        if (LL_TIM_IsActiveFlag_UPDATE(timer) != 0U) {
            LL_TIM_ClearFlag_UPDATE(timer);
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
    TIM_TypeDef* timer = state->config->registers;
    if ((*PWM_GATE_REGISTER & (1U << state->gate_shift)) != 0U &&
        load_preloads(timer)) {
        const uint32_t end = state->config->period_ticks - 1U;
        LL_TIM_SetCounter(timer, end);
        nx_arch_dsb();
        if (LL_TIM_GetCounter(timer) == end &&
            nx_stm32f1_write_masked(&timer->CCER, TIM_CCER_CC1E,
                                    TIM_CCER_CC1E) &&
            master_output(timer, true) && connect_pin(state->config, true) &&
            nx_stm32f1_write_masked(&timer->CR1, TIM_CR1_CEN, TIM_CR1_CEN)) {
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
