/**
 * \file            tim4.c
 * \brief           F103 TIM4 CH1 rising capture on PB6, with EXTI6 witness.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/timer_capture/instance.h"
#include "private/hardware.h"
#include "system/platform.h"

#ifndef NX_STM32F1_CAPTURE_CCR
#define NX_STM32F1_CAPTURE_CCR() (TIM4->CCR1)
#endif

#define CAPTURE_PIN         (UINT32_C(1) << 6U)
#define CAPTURE_PAD_MASK    (UINT32_C(0xF) << 24U)
#define CAPTURE_PAD_FLOAT   (UINT32_C(4) << 24U)
#define CAPTURE_SHARED_EXTI UINT32_C(0x3E0)
#define CAPTURE_FLAGS       ((uint32_t)(TIM_SR_CC1IF | TIM_SR_CC1OF))
#define CAPTURE_KEEP_FLAGS                                                     \
    ((uint32_t)(TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC2IF | TIM_SR_CC3IF |      \
                TIM_SR_CC4IF | TIM_SR_TIF | TIM_SR_CC2OF | TIM_SR_CC3OF |      \
                TIM_SR_CC4OF))
#define CAPTURE_HZ UINT32_C(1000000)

enum capture_effect {
    TIMER_GATE = 1U,
    TIMER_OWNED = 2U,
    AFIO_GATE = 4U,
    MUX_OWNED = 8U,
    PRIORITY_OWNED = 16U,
    LIVE = 32U,
    SESSION = 64U,
    DRAINING = 128U
};

/** \brief Only timer-clock HSI8 references may construct this fixed route. */
static bool clock_ready(void) {
    return nx_stm32f1_system_live() &&
           (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) ==
               RCC_CFGR_SWS_HSI;
}

/** \brief A disabled EXTI vector can record a witness without dispatching. */
static bool witness_available(void) {
    return (EXTI->IMR & (CAPTURE_SHARED_EXTI & ~CAPTURE_PIN)) == 0U &&
           NVIC_GetEnableIRQ(EXTI9_5_IRQn) == 0U &&
           NVIC_GetActive(EXTI9_5_IRQn) == 0U;
}

/** \brief Check a typed CPU span without touching provider or peripheral MMIO.
 */
static bool writable(const nx_stm32f1_tim4_capture_t* state, const void* value,
                     size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(value, bytes, alignment) &&
           !nx_io_dispatch_overlap(value, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(value, bytes,
                                   (const void*)UINT32_C(0x40000000),
                                   UINT32_C(0x20000000)) &&
           !nx_io_dispatch_overlap(value, bytes, (const void*)SCS_BASE,
                                   UINT32_C(0x1000));
}

/** \brief Use the RCC reset of the exclusively owned timer. */
static bool reset_timer(void) {
    return nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_TIM4RST,
                                   RCC_APB1RSTR_TIM4RST) &&
           nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_TIM4RST, 0U);
}

/** \brief Record saturated loss evidence and apply it to the next retained
 * event. */
static void note_loss(nx_stm32f1_tim4_capture_t* state, uint32_t flag) {
    if (state->loss_incidents != UINT32_MAX) {
        ++state->loss_incidents;
    }
    state->pending_flags |= flag;
}

/** \brief Service a single observed record without clearing late CC1IF. */
static void capture(nx_stm32f1_tim4_capture_t* state, bool witness,
                    bool retain) {
    const uint32_t flags = TIM4->SR & CAPTURE_FLAGS;
    if (flags == 0U) {
        return;
    }

    uint32_t ticks = 0U;
    bool suspected_loss = false;
    if ((flags & TIM_SR_CC1IF) != 0U) {
        if (witness) {
            /* ES096 2.6.5: an edge may lose CC1IF and leave CC1OF clear. */
            NX_STM32F1_WRITE(&EXTI->PR, CAPTURE_PIN);
            nx_arch_dsb();
        }
        ticks = NX_STM32F1_CAPTURE_CCR() & UINT16_MAX;
        suspected_loss = witness && (EXTI->PR & CAPTURE_PIN) != 0U;
    }

    if (((flags | TIM4->SR) & TIM_SR_CC1OF) != 0U) {
        NX_STM32F1_WRITE(&TIM4->SR, CAPTURE_KEEP_FLAGS);
        suspected_loss = true;
    }
    if (suspected_loss) {
        /* ES096 2.6.6 permits early CC1OF. This is evidence, not edge count. */
        note_loss(state, NX_TIMER_CAPTURE_OVERCAPTURE);
    }
    if ((flags & TIM_SR_CC1IF) == 0U) {
        return;
    }
    if (!retain || state->count == state->capacity) {
        note_loss(state, NX_TIMER_CAPTURE_RING_LOSS);
        return;
    }
    state->events[state->head] =
        (nx_timer_capture_event_t){.observed_at = nx_time_now_us(),
                                   .captured_ticks = ticks,
                                   .flags = state->pending_flags};
    state->head = (uint16_t)(state->head + 1U);
    if (state->head == state->capacity) {
        state->head = 0U;
    }
    ++state->count;
    state->pending_flags = 0U;
}

/** \brief Disable producers under the caller's saved interrupt mask. */
static nx_result_t detach(nx_stm32f1_tim4_capture_t* state) {
    if ((state->effects & SESSION) != 0U) {
        state->effects |= DRAINING;
        NVIC_DisableIRQ(TIM4_IRQn);
        if ((RCC->APB1ENR & RCC_APB1ENR_TIM4EN) == 0U ||
            !nx_stm32f1_write_masked(&TIM4->DIER, TIM_DIER_CC1IE, 0U) ||
            !nx_stm32f1_write_masked(&TIM4->CCER, TIM_CCER_CC1E, 0U) ||
            !nx_stm32f1_write_masked(&TIM4->CR1, TIM_CR1_CEN, 0U) ||
            !nx_stm32f1_write_masked(&EXTI->IMR, CAPTURE_PIN, 0U) ||
            !nx_stm32f1_write_masked(&EXTI->RTSR, CAPTURE_PIN, 0U) ||
            NVIC_GetEnableIRQ(TIM4_IRQn) != 0U ||
            NVIC_GetActive(TIM4_IRQn) != 0U || !witness_available()) {
            return NX_ERROR_BUSY;
        }

        capture(state, false, state->events != NULL);
        NX_STM32F1_WRITE(&EXTI->PR, CAPTURE_PIN);
        NVIC_ClearPendingIRQ(TIM4_IRQn);
        NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
        nx_arch_dsb();
        if ((EXTI->PR & CAPTURE_PIN) != 0U ||
            NVIC_GetPendingIRQ(TIM4_IRQn) != 0U ||
            NVIC_GetPendingIRQ(EXTI9_5_IRQn) != 0U) {
            return NX_ERROR_BUSY;
        }
        state->effects &= (uint8_t)~SESSION;
    }
    if (state->count != 0U) {
        return NX_ERROR_BUSY;
    }
    state->events = NULL;
    state->effects &= (uint8_t)~DRAINING;
    return NX_SUCCESS;
}

/** \brief Guard start with exactly the configured pad, clock and vector. */
static nx_result_t start(void* context, nx_timer_capture_event_t* events,
                         size_t capacity) {
    nx_stm32f1_tim4_capture_t* state = context;
    if (state == NULL || capacity == 0U || capacity > UINT16_MAX ||
        !writable(state, events, capacity * sizeof(*events),
                  _Alignof(nx_timer_capture_event_t))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & LIVE) == 0U || !clock_ready()) {
        return NX_ERROR_STATE;
    }
    if ((state->effects & (SESSION | DRAINING)) != 0U || !witness_available() ||
        NVIC_GetPendingIRQ(EXTI9_5_IRQn) != 0U ||
        NVIC_GetEnableIRQ(TIM4_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIM4_IRQn) != 0U ||
        NVIC_GetActive(TIM4_IRQn) != 0U) {
        return NX_ERROR_BUSY;
    }
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM4EN) == 0U ||
        (RCC->APB2ENR & (RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN)) !=
            (RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN) ||
        (GPIOB->CRL & CAPTURE_PAD_MASK) != CAPTURE_PAD_FLOAT ||
        (AFIO->MAPR & AFIO_MAPR_TIM4_REMAP) != 0U ||
        (AFIO->EXTICR[1] & AFIO_EXTICR2_EXTI6) != AFIO_EXTICR2_EXTI6_PB ||
        ((EXTI->IMR | EXTI->EMR | EXTI->FTSR | EXTI->RTSR | EXTI->PR) &
         CAPTURE_PIN) != 0U ||
        TIM4->CR1 != 0U || TIM4->CR2 != 0U || TIM4->SMCR != 0U ||
        TIM4->DIER != 0U || TIM4->CCER != 0U || TIM4->PSC != 7U ||
        TIM4->ARR != UINT16_MAX || TIM4->CCMR1 != TIM_CCMR1_CC1S_0 ||
        TIM4->CCMR2 != 0U) {
        return NX_ERROR_STATE;
    }

    nx_arch_irq_state_t previous = nx_arch_irq_save();
    state->effects |= SESSION;
    state->loss_incidents = 0U;
    state->pending_flags = 0U;
    state->head = state->tail = state->count = 0U;
    NX_STM32F1_WRITE(&TIM4->SR, 0U);
    NX_STM32F1_WRITE(&TIM4->CNT, 0U);
    if (TIM4->SR == 0U && TIM4->CNT == 0U &&
        nx_stm32f1_write_masked(&EXTI->RTSR, CAPTURE_PIN, CAPTURE_PIN) &&
        nx_stm32f1_write_masked(&EXTI->IMR, CAPTURE_PIN, CAPTURE_PIN) &&
        nx_stm32f1_write_masked(&TIM4->CR1, TIM_CR1_CEN, TIM_CR1_CEN) &&
        nx_stm32f1_write_masked(&TIM4->CCER, TIM_CCER_CC1E, TIM_CCER_CC1E) &&
        nx_stm32f1_write_masked(&TIM4->DIER, TIM_DIER_CC1IE, TIM_DIER_CC1IE)) {
        NVIC_EnableIRQ(TIM4_IRQn);
        if (NVIC_GetEnableIRQ(TIM4_IRQn) != 0U) {
            state->capacity = (uint16_t)capacity;
            state->events = events;
            nx_arch_irq_restore(previous);
            return NX_SUCCESS;
        }
    }
    (void)detach(state);
    nx_arch_irq_restore(previous);
    return NX_ERROR_IO;
}

/** \brief Retain caller storage on every incomplete detach. */
static nx_result_t stop(void* context) {
    nx_stm32f1_tim4_capture_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    nx_result_t result = detach(state);
    nx_arch_irq_restore(previous);
    return result;
}

/** \brief The task copies only published data from its borrowed event ring. */
static nx_result_t read_event(void* context, nx_timer_capture_event_t* event) {
    nx_stm32f1_tim4_capture_t* state = context;
    if (state == NULL || !writable(state, event, sizeof(*event),
                                   _Alignof(nx_timer_capture_event_t))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_EMPTY;
    if (state->events == NULL) {
        result = NX_ERROR_STATE;
    } else if (nx_io_dispatch_overlap(event, sizeof(*event), state->events,
                                      state->capacity * sizeof(*event))) {
        result = NX_ERROR_INVALID;
    } else if (state->count != 0U) {
        *event = state->events[state->tail];
        state->tail = (uint16_t)(state->tail + 1U);
        if (state->tail == state->capacity) {
            state->tail = 0U;
        }
        --state->count;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(previous);
    return result;
}

/** \brief Report the nominal counter unit and observed queue/loss evidence. */
static nx_result_t query(const void* context, nx_timer_capture_state_t* out) {
    const nx_stm32f1_tim4_capture_t* state = context;
    if (state == NULL || !writable(state, out, sizeof(*out),
                                   _Alignof(nx_timer_capture_state_t))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & LIVE) == 0U) {
        return NX_ERROR_STATE;
    }
    if (state->events != NULL &&
        nx_io_dispatch_overlap(out, sizeof(*out), state->events,
                               state->capacity * sizeof(*state->events))) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    *out = (nx_timer_capture_state_t){
        .tick_hz = CAPTURE_HZ,
        .loss_incidents = state->loss_incidents,
        .pending = state->count,
        .counter_bits = 16U,
        .running = (state->effects & (SESSION | DRAINING)) == SESSION,
        .draining = (state->effects & DRAINING) != 0U};
    nx_arch_irq_restore(previous);
    return NX_SUCCESS;
}

const nx_timer_capture_ops_t nx_stm32f1_tim4_capture_ops = {
    .start = start, .stop = stop, .read = read_event, .state = query};

void nx_stm32f1_tim4_capture_irq(nx_stm32f1_tim4_capture_t* state) {
    if (state != NULL && (state->effects & SESSION) != 0U &&
        (RCC->APB1ENR & RCC_APB1ENR_TIM4EN) != 0U &&
        (TIM4->DIER & TIM_DIER_CC1IE) != 0U) {
        capture(state, true,
                state->events != NULL && (state->effects & DRAINING) == 0U);
    }
}

nx_result_t nx_stm32f1_tim4_capture_close(nx_stm32f1_tim4_capture_t* state) {
    nx_result_t result = stop(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & TIMER_OWNED) != 0U) {
        if (!reset_timer()) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~TIMER_OWNED;
    }
    if ((state->effects & TIMER_GATE) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM4EN, 0U)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~TIMER_GATE;
    }
    if ((state->effects & PRIORITY_OWNED) != 0U) {
        NVIC_SetPriority(TIM4_IRQn, state->saved_priority);
        if (NVIC_GetPriority(TIM4_IRQn) != state->saved_priority) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~PRIORITY_OWNED;
    }
    if ((state->effects & MUX_OWNED) != 0U) {
        if (!nx_stm32f1_write_masked(&AFIO->EXTICR[1], AFIO_EXTICR2_EXTI6,
                                     state->saved_exti)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~MUX_OWNED;
    }
    result = nx_stm32f1_pads_restore(&state->pads);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((state->effects & AFIO_GATE) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
        return NX_ERROR_IO;
    }
    state->effects = 0U;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_tim4_capture_prepare(nx_stm32f1_tim4_capture_t* state,
                                            uint32_t tick_hz,
                                            uint32_t priority) {
    if (state == NULL || tick_hz != CAPTURE_HZ ||
        priority >= (1UL << __NVIC_PRIO_BITS)) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->effects != 0U || !clock_ready()) {
        return NX_ERROR_STATE;
    }
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM4EN) != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_TIM4RST) != 0U || !witness_available() ||
        NVIC_GetPendingIRQ(EXTI9_5_IRQn) != 0U ||
        NVIC_GetEnableIRQ(TIM4_IRQn) != 0U || NVIC_GetActive(TIM4_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIM4_IRQn) != 0U ||
        ((EXTI->IMR | EXTI->EMR | EXTI->FTSR | EXTI->RTSR | EXTI->PR |
          EXTI->SWIER) &
         CAPTURE_PIN) != 0U) {
        return NX_ERROR_BUSY;
    }

    if ((RCC->APB2ENR & RCC_APB2ENR_AFIOEN) == 0U) {
        state->effects |= AFIO_GATE;
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN,
                                     RCC_APB2ENR_AFIOEN)) {
            goto fail;
        }
    }
    if ((AFIO->MAPR & AFIO_MAPR_TIM4_REMAP) != 0U) {
        (void)nx_stm32f1_tim4_capture_close(state);
        return NX_ERROR_BUSY;
    }
    if (nx_stm32f1_pads_capture(&state->pads, GPIOB, CAPTURE_PIN) !=
            NX_SUCCESS ||
        nx_stm32f1_pads_apply(&state->pads, CAPTURE_PAD_FLOAT, 0U,
                              state->pads.latch) != NX_SUCCESS) {
        goto fail;
    }
    state->saved_exti = (uint16_t)(AFIO->EXTICR[1] & AFIO_EXTICR2_EXTI6);
    state->effects |= MUX_OWNED;
    if (!nx_stm32f1_write_masked(&AFIO->EXTICR[1], AFIO_EXTICR2_EXTI6,
                                 AFIO_EXTICR2_EXTI6_PB)) {
        goto fail;
    }
    state->saved_priority = (uint8_t)NVIC_GetPriority(TIM4_IRQn);
    state->effects |= PRIORITY_OWNED;
    NVIC_SetPriority(TIM4_IRQn, priority);
    if (NVIC_GetPriority(TIM4_IRQn) != priority) {
        goto fail;
    }
    state->effects |= TIMER_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM4EN,
                                 RCC_APB1ENR_TIM4EN)) {
        goto fail;
    }
    state->effects |= TIMER_OWNED;
    if (!reset_timer() ||
        !nx_stm32f1_write_masked(&TIM4->CCMR1, TIM_CCMR1_CC1S,
                                 TIM_CCMR1_CC1S_0) ||
        !nx_stm32f1_write_masked(&TIM4->PSC, UINT16_MAX, 7U) ||
        !nx_stm32f1_write_masked(&TIM4->CR2, TIM_CR2_MMS, TIM_CR2_MMS_0)) {
        goto fail;
    }
    /* Load PSC while MMS selects disabled CEN, so UG cannot pulse TRGO. */
    NX_STM32F1_WRITE(&TIM4->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if (!nx_stm32f1_write_masked(&TIM4->CR2, TIM_CR2_MMS, 0U) ||
        (TIM4->SR & TIM_SR_UIF) == 0U || TIM4->ARR != UINT16_MAX) {
        goto fail;
    }
    NX_STM32F1_WRITE(&TIM4->SR, 0U);
    nx_arch_dsb();
    if (TIM4->SR != 0U) {
        goto fail;
    }
    state->effects |= LIVE;
    return NX_SUCCESS;

fail:
    (void)nx_stm32f1_tim4_capture_close(state);
    return NX_ERROR_IO;
}
