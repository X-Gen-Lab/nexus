/**
 * \file            exti.c
 * \brief           F103 PC13 edge events and verified EXTI source withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_exti.h"
#include "stm32f1xx_ll_gpio.h"
#include "system/platform.h"

#ifndef NX_STM32F1_EXTI_ACK
#define NX_STM32F1_EXTI_ACK(mask) LL_EXTI_ClearFlag_0_31(mask)
#endif

#ifndef NX_STM32F1_EXTI_IRQ_ENABLED
#define NX_STM32F1_EXTI_IRQ_ENABLED()  NVIC_GetEnableIRQ(EXTI15_10_IRQn)
#define NX_STM32F1_EXTI_IRQ_PENDING()  NVIC_GetPendingIRQ(EXTI15_10_IRQn)
#define NX_STM32F1_EXTI_IRQ_ACTIVE()   NVIC_GetActive(EXTI15_10_IRQn)
#define NX_STM32F1_EXTI_IRQ_PRIORITY() NVIC_GetPriority(EXTI15_10_IRQn)
#define NX_STM32F1_EXTI_SET_PRIORITY(value)                                    \
    NVIC_SetPriority(EXTI15_10_IRQn, (value))
#define NX_STM32F1_EXTI_IRQ_ENABLE()  NVIC_EnableIRQ(EXTI15_10_IRQn)
#define NX_STM32F1_EXTI_IRQ_DISABLE() NVIC_DisableIRQ(EXTI15_10_IRQn)
#define NX_STM32F1_EXTI_IRQ_CLEAR()   NVIC_ClearPendingIRQ(EXTI15_10_IRQn)
#endif

#define EXTI_LINE       ((uint32_t)LL_EXTI_LINE_13)
#define OTHER_IRQ_LINES (UINT32_C(0xFC00) & ~EXTI_LINE)

/** \brief Only the externally biased PC13 source and exclusive vector qualify.
 */
nx_result_t nx_stm32f1_exti_admit(const nx_stm32f1_exti_t* state,
                                  const nx_stm32f1_exti_config_t* config) {
    if (state == NULL || config == NULL || config->gpio != GPIOC ||
        config->gpio_index != 2U || config->line != 13U || config->pull != 0U ||
        config->edge < NX_EXTI_RISING || config->edge > NX_EXTI_BOTH ||
        config->priority >= (1U << __NVIC_PRIO_BITS) ||
        config->capacity == 0U || config->capacity > 4096U) {
        return NX_ERROR_INVALID;
    }
    const size_t bytes = config->capacity * sizeof(*config->storage);
    if (!nx_io_dispatch_span(config->storage, bytes,
                             _Alignof(nx_exti_event_t)) ||
        nx_io_dispatch_overlap(config->storage, bytes, state, sizeof(*state)) ||
        nx_io_dispatch_overlap(config->storage, bytes, config,
                               sizeof(*config))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config != NULL) {
        return NX_ERROR_STATE;
    }
    const uint32_t existing = EXTI->EMR | EXTI->RTSR | EXTI->FTSR | EXTI->PR;
    return (existing & EXTI_LINE) != 0U ||
                   (EXTI->IMR & (OTHER_IRQ_LINES | EXTI_LINE)) != 0U ||
                   NX_STM32F1_EXTI_IRQ_ENABLED() ||
                   NX_STM32F1_EXTI_IRQ_PENDING() || NX_STM32F1_EXTI_IRQ_ACTIVE()
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}

/** \brief Acquire pin and AFIO receipts before publishing the IRQ producer. */
nx_result_t nx_stm32f1_exti_prepare(nx_stm32f1_exti_t* state,
                                    const nx_stm32f1_exti_config_t* config) {
    nx_result_t result = nx_stm32f1_exti_admit(state, config);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    state->config = config;
    const uint32_t afio_gate = RCC_APB2ENR_AFIOEN;
    state->afio_added = (RCC->APB2ENR & afio_gate) == 0U;
    if (state->afio_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, afio_gate, afio_gate)) {
        return NX_ERROR_IO;
    }
    result = nx_stm32f1_pads_capture(&state->pads, config->gpio,
                                     (uint16_t)EXTI_LINE);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint32_t floating = LL_GPIO_MODE_FLOATING << (4U * (13U - 8U));
    result =
        nx_stm32f1_pads_apply(&state->pads, 0U, floating, state->pads.latch);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint32_t route = AFIO_EXTICR4_EXTI13;
    state->previous_route = AFIO->EXTICR[3] & route;
    state->mux_owned = true;
    if (!nx_stm32f1_write_masked(&AFIO->EXTICR[3], route,
                                 AFIO_EXTICR4_EXTI13_PC)) {
        return NX_ERROR_IO;
    }
    state->line_owned = true;
    if (!nx_stm32f1_write_masked(
            &EXTI->RTSR, EXTI_LINE,
            (config->edge & NX_EXTI_RISING) != 0U ? EXTI_LINE : 0U) ||
        !nx_stm32f1_write_masked(
            &EXTI->FTSR, EXTI_LINE,
            (config->edge & NX_EXTI_FALLING) != 0U ? EXTI_LINE : 0U)) {
        return NX_ERROR_IO;
    }
    NX_STM32F1_EXTI_ACK(EXTI_LINE);
    nx_arch_dsb();
    if ((EXTI->PR & EXTI_LINE) != 0U) {
        return NX_ERROR_IO;
    }
    state->previous_priority = (uint8_t)NX_STM32F1_EXTI_IRQ_PRIORITY();
    state->vector_owned = true;
    NX_STM32F1_EXTI_SET_PRIORITY(config->priority);
    if (NX_STM32F1_EXTI_IRQ_PRIORITY() != config->priority ||
        !nx_stm32f1_write_masked(&EXTI->IMR, EXTI_LINE, EXTI_LINE)) {
        return NX_ERROR_IO;
    }
    state->live = true;
    NX_STM32F1_EXTI_IRQ_ENABLE();
    if (!NX_STM32F1_EXTI_IRQ_ENABLED()) {
        state->live = false;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Acknowledge only the configured cause, preserving sibling PR bits. */
void nx_stm32f1_exti_irq(nx_stm32f1_exti_t* state) {
    if (!state->live || (EXTI->PR & EXTI->IMR & EXTI_LINE) == 0U) {
        return;
    }
    NX_STM32F1_EXTI_ACK(EXTI_LINE);
    const nx_stm32f1_exti_config_t* config = state->config;
    if (state->count == config->capacity || state->loss) {
        state->loss = true;
        return;
    }
    nx_exti_edge_t edge = config->edge;
    if (edge == NX_EXTI_BOTH) {
        edge = (LL_GPIO_ReadInputPort(config->gpio) & EXTI_LINE) != 0U
                   ? NX_EXTI_RISING
                   : NX_EXTI_FALLING;
    }
    size_t tail = (size_t)state->head + state->count;
    if (tail >= config->capacity) {
        tail -= config->capacity;
    }
    config->storage[tail] = (nx_exti_event_t){
        .timestamp_us = nx_time_now_us(),
        .flags = NX_EXTI_EVENT_COALESCED,
        .edge = edge,
    };
    ++state->count;
}

/** \brief Outputs may never replace live provider storage or MMIO. */
static bool independent(const nx_stm32f1_exti_t* state, const void* data,
                        size_t bytes, size_t alignment) {
    const nx_stm32f1_exti_config_t* config = state->config;
    return nx_io_dispatch_span(data, bytes, alignment) &&
           !nx_io_dispatch_overlap(data, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(data, bytes, config, sizeof(*config)) &&
           !nx_io_dispatch_overlap(data, bytes, config->storage,
                                   config->capacity *
                                       sizeof(*config->storage)) &&
           !nx_io_dispatch_overlap(data, bytes, config->gpio,
                                   sizeof(*config->gpio)) &&
           !nx_io_dispatch_overlap(data, bytes, EXTI, sizeof(*EXTI)) &&
           !nx_io_dispatch_overlap(data, bytes, AFIO, sizeof(*AFIO)) &&
           !nx_io_dispatch_overlap(data, bytes, RCC, sizeof(*RCC));
}

/** \brief Drain older facts before publishing the ordered loss boundary. */
static nx_result_t read_events(void* context, nx_exti_event_t* events,
                               size_t capacity, size_t* count) {
    nx_stm32f1_exti_t* state = context;
    if (!nx_arch_is_privileged() || nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->live) {
        return NX_ERROR_STATE;
    }
    if (capacity == 0U || capacity > SIZE_MAX / sizeof(*events) ||
        !independent(state, events, capacity * sizeof(*events),
                     _Alignof(nx_exti_event_t)) ||
        !independent(state, count, sizeof(*count), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(events, capacity * sizeof(*events), count,
                               sizeof(*count))) {
        return NX_ERROR_INVALID;
    }
    size_t copied = 0U;
    while (copied < capacity) {
        const nx_arch_irq_state_t mask = nx_arch_irq_save();
        if (state->count == 0U) {
            const bool lost = state->loss;
            state->loss = false;
            nx_arch_irq_restore(mask);
            if (lost) {
                events[copied++] = (nx_exti_event_t){
                    .timestamp_us = nx_time_now_us(),
                    .flags = NX_EXTI_EVENT_LOSS,
                };
            }
            break;
        }
        const nx_exti_event_t event = state->config->storage[state->head];
        if (++state->head == state->config->capacity) {
            state->head = 0U;
        }
        --state->count;
        nx_arch_irq_restore(mask);
        events[copied++] = event;
    }
    *count = copied;
    return copied == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}

/** \brief Sample the same input without touching pending state or the ring. */
static nx_result_t read_level(const void* context, bool* high) {
    const nx_stm32f1_exti_t* state = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->live) {
        return NX_ERROR_STATE;
    }
    if (!independent(state, high, sizeof(*high), _Alignof(bool))) {
        return NX_ERROR_INVALID;
    }
    *high = (LL_GPIO_ReadInputPort(state->config->gpio) & EXTI_LINE) != 0U;
    return NX_SUCCESS;
}

/** \brief Mask the source and remove both edge generators before PR drain. */
static bool withdraw_line(void) {
    const bool masked = nx_stm32f1_write_masked(&EXTI->IMR, EXTI_LINE, 0U);
    const bool rising_off = nx_stm32f1_write_masked(&EXTI->RTSR, EXTI_LINE, 0U);
    const bool falling_off =
        nx_stm32f1_write_masked(&EXTI->FTSR, EXTI_LINE, 0U);
    if (!masked || !rising_off || !falling_off) {
        return false;
    }
    NX_STM32F1_EXTI_ACK(EXTI_LINE);
    nx_arch_dsb();
    return (EXTI->PR & EXTI_LINE) == 0U;
}

/** \brief End event/level access while retaining cold receipts for Platform. */
static nx_result_t stop(void* context) {
    nx_stm32f1_exti_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->live = false;
    const bool foreign = (EXTI->IMR & OTHER_IRQ_LINES) != 0U;
    if (state->vector_owned && !foreign) {
        NX_STM32F1_EXTI_IRQ_DISABLE();
        nx_arch_dsb();
    }
    if (state->line_owned && withdraw_line()) {
        state->line_owned = false;
    }
    nx_result_t result = state->line_owned ? NX_ERROR_IO : NX_SUCCESS;
    if (result == NX_SUCCESS && state->vector_owned) {
        if (foreign) {
            result = NX_ERROR_BUSY;
        } else if (NX_STM32F1_EXTI_IRQ_ENABLED() ||
                   NX_STM32F1_EXTI_IRQ_ACTIVE()) {
            result = NX_ERROR_IO;
        } else {
            NX_STM32F1_EXTI_IRQ_CLEAR();
            NX_STM32F1_EXTI_SET_PRIORITY(state->previous_priority);
            nx_arch_dsb();
            if (NX_STM32F1_EXTI_IRQ_PENDING() ||
                NX_STM32F1_EXTI_IRQ_PRIORITY() != state->previous_priority) {
                result = NX_ERROR_IO;
            } else {
                state->vector_owned = false;
            }
        }
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Return AFIO routing, pins and originally acquired clocks in order. */
nx_result_t nx_stm32f1_exti_close(nx_stm32f1_exti_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    const nx_result_t stopped = stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL) {
        return stopped;
    }
    if (state->mux_owned &&
        !nx_stm32f1_write_masked(&AFIO->EXTICR[3], AFIO_EXTICR4_EXTI13,
                                 state->previous_route)) {
        return NX_ERROR_IO;
    }
    state->mux_owned = false;
    const nx_result_t restored = nx_stm32f1_pads_restore(&state->pads);
    if (restored != NX_SUCCESS) {
        return restored;
    }
    if (state->afio_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
        return NX_ERROR_IO;
    }
    *state = (nx_stm32f1_exti_t){0};
    return NX_SUCCESS;
}

const nx_exti_ops_t nx_stm32f1_exti_ops = {
    .read = read_events, .stop = stop, .level = read_level};
