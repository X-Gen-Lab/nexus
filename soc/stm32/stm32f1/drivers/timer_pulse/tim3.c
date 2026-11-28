/**
 * \file            tim3.c
 * \brief           F103 TIM3 CH1 hardware one-pulse output on PA6.
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

#if !defined(STM32F103xB)
#error "Selected one-pulse owner requires STM32F103xB"
#endif

#define PULSE_PIN      (UINT32_C(1) << 6U)
#define PULSE_PAD_MASK (UINT32_C(0xF) << 24U)
#define PULSE_PAD_IDLE (UINT32_C(2) << 24U)
#define PULSE_PAD_AF   (UINT32_C(0xA) << 24U)
#define PULSE_HZ       UINT32_C(1000000)
#define PULSE_PSC      UINT32_C(7)
#define PULSE_CR1      ((uint32_t)(TIM_CR1_OPM | TIM_CR1_URS))

enum pulse_effect {
    PULSE_AFIO = 1U,
    PULSE_TIMER = 2U,
    PULSE_PRIORITY = 4U,
    PULSE_LIVE = 8U
};

static bool clock_ready(void) {
    return nx_stm32f1_system_live() &&
           (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) ==
               RCC_CFGR_SWS_HSI;
}

static bool timer_clocked(void) {
    return (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U;
}

static bool reset_timer(void) {
    return nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_TIM3RST,
                                   RCC_APB1RSTR_TIM3RST) &&
           nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_TIM3RST, 0U);
}

static bool release_afio(nx_stm32f1_tim3_pulse_t* state) {
    if ((state->effects & PULSE_AFIO) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U))
            return false;
        state->effects &= (uint8_t)~PULSE_AFIO;
    }
    return true;
}

static bool independent_span(const nx_stm32f1_tim3_pulse_t* state,
                             const void* value, size_t bytes,
                             size_t alignment) {
    return nx_io_dispatch_span(value, bytes, alignment) &&
           !nx_io_dispatch_overlap(value, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(value, bytes,
                                   (const void*)UINT32_C(0x40000000),
                                   UINT32_C(0x20000000)) &&
           !nx_io_dispatch_overlap(value, bytes, (const void*)SCS_BASE,
                                   UINT32_C(0x1000));
}

/** \brief Use the preloaded low latch when leaving alternate output. */
static bool connect_pad(uint32_t mode) {
    return (RCC->APB2ENR & RCC_APB2ENR_IOPAEN) != 0U &&
           (GPIOA->ODR & PULSE_PIN) == 0U &&
           nx_stm32f1_write_masked(&GPIOA->CRL, PULSE_PAD_MASK, mode);
}

static nx_result_t quiet(void) {
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    NVIC_DisableIRQ(TIM3_IRQn);
    /* A frozen PWM2 counter may be high. Park PA6 before stopping it. */
    bool ok = connect_pad(PULSE_PAD_IDLE) && timer_clocked();
    if (ok) {
        NX_STM32F1_WRITE(&TIM3->DIER, 0U);
        NX_STM32F1_WRITE(&TIM3->CR1, PULSE_CR1);
        NX_STM32F1_WRITE(&TIM3->CCER, 0U);
        nx_arch_dsb();
        ok = TIM3->DIER == 0U && TIM3->CR1 == PULSE_CR1 && TIM3->CCER == 0U &&
             NVIC_GetEnableIRQ(TIM3_IRQn) == 0U &&
             NVIC_GetActive(TIM3_IRQn) == 0U;
    }
    if (ok) {
        /* This owner reserves the entire timer and its update vector. */
        NX_STM32F1_WRITE(&TIM3->SR, 0U);
        NVIC_ClearPendingIRQ(TIM3_IRQn);
        nx_arch_dsb();
        ok = TIM3->SR == 0U && NVIC_GetPendingIRQ(TIM3_IRQn) == 0U;
    }
    nx_arch_irq_restore(saved);
    return ok ? NX_SUCCESS : NX_ERROR_IO;
}

static bool completed(void) {
    return (TIM3->SR & TIM_SR_UIF) != 0U && TIM3->CR1 == PULSE_CR1 &&
           TIM3->CNT == 0U;
}

static bool arm_output(uint32_t period, uint32_t width) {
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    const uint32_t mode =
        width == 0U ? LL_TIM_OCMODE_FORCED_INACTIVE : LL_TIM_OCMODE_PWM2;
    const uint32_t compare = width == 0U ? 1U : period - width;
    LL_TIM_OC_SetCompareCH1(TIM3, compare);
    NX_STM32F1_WRITE(&TIM3->CCMR1, mode);
    NX_STM32F1_WRITE(&TIM3->EGR, TIM_EGR_UG);
    NX_STM32F1_WRITE(&TIM3->SR, 0U);
    nx_arch_dsb();
    bool ok = TIM3->CR1 == PULSE_CR1 && TIM3->CNT == 0U && TIM3->SR == 0U &&
              TIM3->CCR1 == compare && TIM3->CCMR1 == mode;
    if (ok) {
        NX_STM32F1_WRITE(&TIM3->CCER, TIM_CCER_CC1E);
        nx_arch_dsb();
        ok = TIM3->CCER == TIM_CCER_CC1E && connect_pad(PULSE_PAD_AF);
    }
    if (ok) {
        NVIC_EnableIRQ(TIM3_IRQn);
        NX_STM32F1_WRITE(&TIM3->DIER, TIM_DIER_UIE);
        nx_arch_dsb();
        ok = NVIC_GetEnableIRQ(TIM3_IRQn) != 0U && TIM3->DIER == TIM_DIER_UIE;
    }
    if (ok) {
        NX_STM32F1_WRITE(&TIM3->CR1, PULSE_CR1 | TIM_CR1_CEN);
        nx_arch_dsb();
        /* A very short valid pulse can finish before this readback. */
        ok = TIM3->CR1 == (PULSE_CR1 | TIM_CR1_CEN) || completed();
    }
    nx_arch_irq_restore(saved);
    return ok;
}

/** \brief Preserve UIF as hardware evidence and stop repeated IRQ dispatch. */
void nx_stm32f1_tim3_pulse_irq(void) {
    NVIC_DisableIRQ(TIM3_IRQn);
}

static bool live(const nx_stm32f1_tim3_pulse_t* state) {
    return state != NULL && (state->effects & PULSE_LIVE) != 0U;
}

static bool faulted(const nx_stm32f1_tim3_pulse_t* state) {
    return state->result == NX_ERROR_IO || state->result == NX_ERROR_STATE;
}

static nx_result_t quarantine(nx_stm32f1_tim3_pulse_t* state,
                              nx_result_t error) {
    if (state->result == NX_SUCCESS)
        state->result = error;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_QUARANTINED);
    return error;
}

static nx_result_t submit(void* context, nx_timer_pulse_request_t* request) {
    nx_stm32f1_tim3_pulse_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!live(state))
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    if (!independent_span(state, request, sizeof(*request),
                          _Alignof(nx_timer_pulse_request_t)) ||
        request->channel_count != 1U ||
        !independent_span(state, request->width_ticks, sizeof(uint32_t),
                          _Alignof(uint32_t)) ||
        nx_io_dispatch_overlap(request, sizeof(*request), request->width_ticks,
                               sizeof(uint32_t)))
        return NX_ERROR_INVALID;
    if (!clock_ready() || !timer_clocked() || faulted(state) ||
        TIM3->CR1 != PULSE_CR1 || TIM3->DIER != 0U || TIM3->CCER != 0U ||
        NVIC_GetEnableIRQ(TIM3_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIM3_IRQn) != 0U || NVIC_GetActive(TIM3_IRQn) != 0U)
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    const nx_time_us_t now = nx_time_now_us();
    const uint32_t width = request->width_ticks[0];
    if (width >= state->period_ticks ||
        nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;

    state->active = request;
    state->observed_at = now;
    state->result =
        arm_output(state->period_ticks, width) ? NX_SUCCESS : NX_ERROR_IO;
    return NX_SUCCESS;
}

static nx_result_t service(void* context) {
    nx_stm32f1_tim3_pulse_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!live(state))
        return NX_ERROR_STATE;
    nx_timer_pulse_request_t* request = state->active;
    if (request == NULL)
        return NX_SUCCESS;
    if (!clock_ready() || !timer_clocked()) {
        state->result = NX_ERROR_STATE;
        (void)quiet();
        return quarantine(state, NX_ERROR_STATE);
    }
    const nx_time_us_t now = nx_time_now_us();
    if (now < state->observed_at) {
        state->result = NX_ERROR_STATE;
        (void)quiet();
        return quarantine(state, NX_ERROR_STATE);
    }
    state->observed_at = now;
    if (state->result == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, now))
        state->result = NX_ERROR_TIMEOUT;
    if (state->result == NX_SUCCESS && !completed())
        return NX_ERROR_BUSY;
    if (nx_request_state(&request->base) == NX_REQUEST_ACTIVE)
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
    if (quiet() != NX_SUCCESS)
        return quarantine(state, NX_ERROR_IO);
    if (state->result == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        state->result = NX_ERROR_TIMEOUT;
    const nx_result_t result = state->result;
    state->active = NULL;
    nx_request_settle(&request->base, result, result == NX_SUCCESS ? 1U : 0U);
    return result;
}

static nx_result_t cancel(void* context, nx_timer_pulse_request_t* request) {
    nx_stm32f1_tim3_pulse_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!live(state) || request == NULL || state->active != request)
        return NX_ERROR_STATE;
    if (state->result == NX_SUCCESS)
        state->result = NX_ERROR_CANCELLED;
    return service(state);
}

static nx_result_t stop(void* context) {
    nx_stm32f1_tim3_pulse_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!live(state))
        return NX_ERROR_STATE;
    if (state->active != NULL) {
        const nx_result_t result = cancel(state, state->active);
        if (state->active != NULL)
            return result;
    } else if (quiet() != NX_SUCCESS) {
        return NX_ERROR_IO;
    }
    state->result = NX_SUCCESS;
    return NX_SUCCESS;
}

static nx_result_t observe(const void* context, nx_timer_pulse_state_t* out) {
    const nx_stm32f1_tim3_pulse_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!live(state) || !clock_ready() || !timer_clocked())
        return NX_ERROR_STATE;
    if (!independent_span(state, out, sizeof(*out),
                          _Alignof(nx_timer_pulse_state_t)))
        return NX_ERROR_INVALID;
    const nx_timer_pulse_request_t* request = state->active;
    if (request != NULL &&
        (nx_io_dispatch_overlap(out, sizeof(*out), request, sizeof(*request)) ||
         nx_io_dispatch_overlap(out, sizeof(*out), request->width_ticks,
                                sizeof(uint32_t))))
        return NX_ERROR_INVALID;
    *out = (nx_timer_pulse_state_t){
        .tick_hz = PULSE_HZ,
        .period_ticks = state->period_ticks,
        .channel_count = 1U,
        .running = (TIM3->CR1 & TIM_CR1_CEN) != 0U,
        .fault = faulted(state),
        .quarantined = request != NULL && nx_request_state(&request->base) ==
                                              NX_REQUEST_QUARANTINED};
    return NX_SUCCESS;
}

const nx_timer_pulse_ops_t nx_stm32f1_tim3_pulse_ops = {.submit = submit,
                                                        .cancel = cancel,
                                                        .service = service,
                                                        .stop = stop,
                                                        .state = observe};

nx_result_t nx_stm32f1_tim3_pulse_prepare(nx_stm32f1_tim3_pulse_t* state,
                                          uint32_t tick_hz, uint32_t period,
                                          uint32_t priority) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || tick_hz != PULSE_HZ || period < 2U ||
        period > UINT32_C(65536) || priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    if (!clock_ready())
        return NX_ERROR_UNSUPPORTED;
    if (state->effects != 0U || state->pads.registers != NULL ||
        timer_clocked() || (RCC->APB1RSTR & RCC_APB1RSTR_TIM3RST) != 0U ||
        NVIC_GetEnableIRQ(TIM3_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIM3_IRQn) != 0U || NVIC_GetActive(TIM3_IRQn) != 0U)
        return NX_ERROR_BUSY;
    if ((RCC->APB2ENR & RCC_APB2ENR_AFIOEN) == 0U) {
        state->effects |= PULSE_AFIO;
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN,
                                     RCC_APB2ENR_AFIOEN))
            return NX_ERROR_IO;
    }
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_TIM3_REMAP) != 0U;
    if (!release_afio(state))
        return NX_ERROR_IO;
    if (remapped)
        return NX_ERROR_UNSUPPORTED;

    nx_result_t result =
        nx_stm32f1_pads_capture(&state->pads, GPIOA, PULSE_PIN);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_pads_apply(&state->pads, PULSE_PAD_IDLE, 0U, 0U);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM3EN,
                                 RCC_APB1ENR_TIM3EN))
        return NX_ERROR_IO;
    state->effects |= PULSE_TIMER;
    if (!reset_timer())
        return NX_ERROR_IO;

    LL_TIM_SetPrescaler(TIM3, PULSE_PSC);
    LL_TIM_SetAutoReload(TIM3, period - 1U);
    NX_STM32F1_WRITE(&TIM3->CR1, PULSE_CR1);
    NX_STM32F1_WRITE(&TIM3->EGR, TIM_EGR_UG);
    NX_STM32F1_WRITE(&TIM3->SR, 0U);
    nx_arch_dsb();
    if (TIM3->CR1 != PULSE_CR1 || TIM3->PSC != PULSE_PSC ||
        TIM3->ARR != period - 1U || TIM3->CNT != 0U || TIM3->SR != 0U ||
        TIM3->DIER != 0U || TIM3->CCER != 0U || TIM3->SMCR != 0U)
        return NX_ERROR_IO;

    state->saved_priority = (uint8_t)NVIC_GetPriority(TIM3_IRQn);
    state->effects |= PULSE_PRIORITY;
    NVIC_SetPriority(TIM3_IRQn, priority);
    if (NVIC_GetPriority(TIM3_IRQn) != priority)
        return NX_ERROR_IO;
    state->period_ticks = period;
    state->result = NX_SUCCESS;
    state->effects |= PULSE_LIVE;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_tim3_pulse_close(nx_stm32f1_tim3_pulse_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (live(state)) {
        const nx_result_t result = stop(state);
        if (result != NX_SUCCESS)
            return result;
        state->effects &= (uint8_t)~PULSE_LIVE;
    }
    if ((state->effects & PULSE_PRIORITY) != 0U) {
        NVIC_SetPriority(TIM3_IRQn, state->saved_priority);
        if (NVIC_GetPriority(TIM3_IRQn) != state->saved_priority)
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~PULSE_PRIORITY;
    }
    if ((state->effects & PULSE_TIMER) != 0U) {
        if (!reset_timer() ||
            !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM3EN, 0U))
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~PULSE_TIMER;
    }
    if (!release_afio(state))
        return NX_ERROR_IO;
    return nx_stm32f1_pads_restore(&state->pads);
}
