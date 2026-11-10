/**
 * \file            timer_capture_impl.h
 * \brief           Compile-time specialized bounded capture implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "timer_advanced_controller.h"
/* One specialization per translation unit. The wrapper owns all required
 * NX bindings, including exports and post-include cold helpers. This template
 * removes only its own helper/default macros; caller-supplied ports survive. */
#if !defined(NX_CAPTURE_CR1) || !defined(NX_CAPTURE_CR2) ||                    \
    !defined(NX_CAPTURE_SMCR) || !defined(NX_CAPTURE_PSC) ||                   \
    !defined(NX_CAPTURE_ARR) || !defined(NX_CAPTURE_CCR1) ||                   \
    !defined(NX_CAPTURE_CCR2) || !defined(NX_CAPTURE_CCR3) ||                  \
    !defined(NX_CAPTURE_CCR4) || !defined(NX_CAPTURE_CCMR1) ||                 \
    !defined(NX_CAPTURE_CCMR2) || !defined(NX_CAPTURE_CCER) ||                 \
    !defined(NX_CAPTURE_DIER) || !defined(NX_CAPTURE_EGR) ||                   \
    !defined(NX_CAPTURE_SR) || !defined(NX_CAPTURE_INITIALIZE) ||              \
    !defined(NX_CAPTURE_IRQ) || !defined(NX_CAPTURE_RELEASE) ||                \
    !defined(NX_CAPTURE_OPS) || !defined(NX_CAPTURE_HZ) ||                     \
    !defined(NX_CAPTURE_CLOCK_READY)
#error "Capture implementation requires one complete concrete specialization"
#endif
#ifndef NX_TIMER_WRITE
#define NX_CAPTURE_DEFAULT_TIMER_WRITE 1
#define NX_TIMER_WRITE(reg, value)     ((reg) = (uint32_t)(value))
#endif
#ifndef NX_TIMER_READ
#define NX_CAPTURE_DEFAULT_TIMER_READ 1
#define NX_TIMER_READ(reg)            ((uint32_t)(reg))
#endif
#ifndef NX_CAPTURE_COUNTER_BITS
#define NX_CAPTURE_DEFAULT_CAPTURE_COUNTER_BITS 1
#define NX_CAPTURE_COUNTER_BITS                 16U
#endif
#if NX_CAPTURE_COUNTER_BITS == 32U
#define NX_CAPTURE_COUNTER_MAX UINT32_MAX
#elif NX_CAPTURE_COUNTER_BITS == 16U
#define NX_CAPTURE_COUNTER_MAX UINT16_MAX
#else
#error "Unsupported capture counter width"
#endif
#ifndef NX_CAPTURE_CONTROL_OVERLAP
#define NX_CAPTURE_DEFAULT_CAPTURE_CONTROL_OVERLAP 1
#define NX_CAPTURE_CONTROL_OVERLAP(pointer, bytes) (false)
#endif
#ifndef NX_CAPTURE_VECTOR_QUIET
#define NX_CAPTURE_DEFAULT_CAPTURE_VECTOR_QUIET 1
#define NX_CAPTURE_VECTOR_QUIET()               (true)
#endif
#ifndef NX_CAPTURE_ACCESS_READY
#define NX_CAPTURE_DEFAULT_CAPTURE_ACCESS_READY 1
#define NX_CAPTURE_ACCESS_READY()               (true)
#endif
#ifndef NX_CAPTURE_CHANNELS
#define NX_CAPTURE_DEFAULT_CHANNELS 1
#define NX_CAPTURE_CHANNELS         4U
#endif
#ifndef NX_CAPTURE_HAS_CR2
#define NX_CAPTURE_DEFAULT_HAS_CR2 1
#define NX_CAPTURE_HAS_CR2         1
#endif
#ifndef NX_CAPTURE_HAS_SMCR
#define NX_CAPTURE_DEFAULT_HAS_SMCR 1
#define NX_CAPTURE_HAS_SMCR         1
#endif
#if NX_CAPTURE_CHANNELS != 1U && NX_CAPTURE_CHANNELS != 2U &&                  \
    NX_CAPTURE_CHANNELS != 4U
#error "Capture channel count must match a reviewed concrete timer"
#endif
/** \brief Select one scalar SDK CCR; no array arithmetic crosses registers. */
static volatile uint32_t* capture_compare(uint8_t channel) {
#if NX_CAPTURE_CHANNELS == 1U
    (void)channel;
    return &NX_CAPTURE_CCR1;
#elif NX_CAPTURE_CHANNELS == 2U
    return channel == 1U ? &NX_CAPTURE_CCR1 : &NX_CAPTURE_CCR2;
#else
    switch (channel) {
        case 1U:
            return &NX_CAPTURE_CCR1;
        case 2U:
            return &NX_CAPTURE_CCR2;
        case 3U:
            return &NX_CAPTURE_CCR3;
        default:
            return &NX_CAPTURE_CCR4;
    }
#endif
}
/** \brief Fixed channel mode byte in its exact CCMR register. */
static uint32_t capture_mode(const nx_timer_capture_config_t* config) {
    return (1U | ((uint32_t)config->filter << 4U))
           << (((uint32_t)config->channel - 1U) % 2U * 8U);
}
/** \brief Input polarity bits follow the selected four-bit CCER group. */
static uint32_t capture_polarity(const nx_timer_capture_config_t* config) {
    uint32_t bits = config->edge == NX_TIMER_EDGE_RISING ? 0U : 2U;
    if (config->edge == NX_TIMER_EDGE_BOTH)
        bits |= 8U;
    return bits << (((uint32_t)config->channel - 1U) * 4U);
}
/** \brief Serialized task controls require the actual privileged CPU context.
 */
static bool capture_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr();
}
/** \brief Overflow-safe overlap query for bounded caller-owned spans. */
static bool capture_overlap(const void* a, size_t an, const void* b,
                            size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (x < y + bn && y < x + an);
}
/** \brief Protect provider/config/ring storage before any task output write. */
static bool capture_output(const nx_timer_capture_controller_t* port,
                           const void* output, size_t bytes) {
    return output != NULL && !NX_CAPTURE_CONTROL_OVERLAP(output, bytes) &&
           !capture_overlap(output, bytes, port, sizeof(*port)) &&
           !capture_overlap(output, bytes, port->config,
                            sizeof(*port->config)) &&
           !capture_overlap(output, bytes, &NX_CAPTURE_OPS,
                            sizeof(NX_CAPTURE_OPS)) &&
           (port->events == NULL ||
            !capture_overlap(output, bytes, port->events,
                             port->capacity * sizeof(*port->events)));
}
/** \brief Saturating observable incidents; coalesced edges remain unknown. */
static void capture_loss(nx_timer_capture_controller_t* port, uint32_t flag) {
    if (port->loss_incidents != UINT32_MAX) {
        ++port->loss_incidents;
    }
    port->pending_flags |= flag;
}
/** \brief Validate every immutable fact before changing the selected timer. */
nx_result_t NX_CAPTURE_INITIALIZE(nx_timer_capture_controller_t* port,
                                  const nx_timer_capture_config_t* config) {
    if (port == NULL || config == NULL ||
        NX_CAPTURE_CONTROL_OVERLAP(port, sizeof(*port)) ||
        NX_CAPTURE_CONTROL_OVERLAP(config, sizeof(*config)) ||
        capture_overlap(port, sizeof(*port), config, sizeof(*config)) ||
        config->channel < 1U || config->channel > NX_CAPTURE_CHANNELS ||
        config->tick_hz == 0U || config->timer_hz != NX_CAPTURE_HZ ||
        config->timer_hz % config->tick_hz != 0U ||
        config->timer_hz / config->tick_hz > 65536U || config->filter > 15U ||
        config->edge > NX_TIMER_EDGE_BOTH) {
        return NX_ERROR_INVALID;
    }
    if (!capture_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!config->exclusive) {
        return NX_ERROR_PERMISSION;
    }
    if (port->config != NULL || port->initialized || port->events != NULL ||
        !NX_CAPTURE_VECTOR_QUIET()) {
        return NX_ERROR_BUSY;
    }
    if (!NX_CAPTURE_CLOCK_READY(config->channel)) {
        return NX_ERROR_STATE;
    }
    if ((NX_CAPTURE_CR1 & 1U) != 0U || NX_CAPTURE_CCER != 0U ||
        NX_CAPTURE_DIER != 0U) {
        return NX_ERROR_BUSY;
    }
    const uint32_t polarity = capture_polarity(config);
    /* Retain the immutable session before the first effect. Failed readback
     * keeps hot controls closed and gives assembly an exact cleanup owner. */
    port->config = config;
    NX_TIMER_WRITE(NX_CAPTURE_CR1, 0U);
#if NX_CAPTURE_HAS_CR2
    NX_TIMER_WRITE(NX_CAPTURE_CR2, 1U << 4U);
#endif
#if NX_CAPTURE_HAS_SMCR
    NX_TIMER_WRITE(NX_CAPTURE_SMCR, 0U);
#endif
    NX_TIMER_WRITE(NX_CAPTURE_PSC, config->timer_hz / config->tick_hz - 1U);
    NX_TIMER_WRITE(NX_CAPTURE_ARR, NX_CAPTURE_COUNTER_MAX);
    NX_TIMER_WRITE(NX_CAPTURE_CCMR1,
                   config->channel <= 2U ? capture_mode(config) : 0U);
#if NX_CAPTURE_CHANNELS > 2U
    NX_TIMER_WRITE(NX_CAPTURE_CCMR2,
                   config->channel > 2U ? capture_mode(config) : 0U);
#endif
    NX_TIMER_WRITE(NX_CAPTURE_CCER, polarity);
    nx_arch_dsb();
    if (NX_CAPTURE_CR1 != 0U ||
#if NX_CAPTURE_HAS_CR2
        NX_CAPTURE_CR2 != (1U << 4U) ||
#endif
        NX_CAPTURE_PSC != config->timer_hz / config->tick_hz - 1U ||
        NX_CAPTURE_ARR != NX_CAPTURE_COUNTER_MAX ||
        NX_CAPTURE_CCMR1 !=
            (config->channel <= 2U ? capture_mode(config) : 0U) ||
#if NX_CAPTURE_CHANNELS > 2U
        NX_CAPTURE_CCMR2 !=
            (config->channel > 2U ? capture_mode(config) : 0U) ||
#endif
#if NX_CAPTURE_HAS_SMCR
        NX_CAPTURE_SMCR != 0U ||
#endif
        NX_CAPTURE_CCER != polarity) {
        return NX_ERROR_IO;
    }
    /* MMS=enable follows disabled CEN while UG loads the prescaler. Prove a
     * fresh UIF: a readable PSC preload or stale UIF is not an active base. */
    NX_TIMER_WRITE(NX_CAPTURE_SR, ~UINT32_C(1));
    nx_arch_dsb();
    if ((NX_CAPTURE_SR & 1U) != 0U) {
        return NX_ERROR_IO;
    }
    NX_TIMER_WRITE(NX_CAPTURE_EGR, 1U);
    nx_arch_dsb();
    if ((NX_CAPTURE_SR & 1U) == 0U) {
        return NX_ERROR_IO;
    }
    NX_TIMER_WRITE(NX_CAPTURE_SR, 0U);
#if NX_CAPTURE_HAS_CR2
    NX_TIMER_WRITE(NX_CAPTURE_CR2, 0U);
#endif
    nx_arch_dsb();
    if ((NX_CAPTURE_SR & 1U) != 0U ||
#if NX_CAPTURE_HAS_CR2
        NX_CAPTURE_CR2 != 0U ||
#endif
        NX_CAPTURE_CR1 != 0U) {
        return NX_ERROR_IO;
    }
    *port =
        (nx_timer_capture_controller_t){.config = config, .initialized = true};
    return NX_SUCCESS;
}
/** \brief Publish the ring before opening its bounded capture IRQ source. */
static nx_result_t capture_start(void* context,
                                 nx_timer_capture_event_t* events,
                                 size_t capacity) {
    nx_timer_capture_controller_t* port = context;
    if (port == NULL || !port->initialized || events == NULL ||
        capacity == 0U || capacity > SIZE_MAX / sizeof(*events) ||
        (uintptr_t)events % _Alignof(nx_timer_capture_event_t) != 0U ||
        !capture_output(port, events, capacity * sizeof(*events))) {
        return NX_ERROR_INVALID;
    }
    if (!capture_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->events != NULL || port->running || port->draining) {
        return NX_ERROR_BUSY;
    }
    const uint32_t polarity = capture_polarity(port->config);
    const uint32_t enable = 1U << (((uint32_t)port->config->channel - 1U) * 4U);
    const uint32_t flag = 1U << port->config->channel;
    if (!NX_CAPTURE_CLOCK_READY(port->config->channel) ||
        NX_CAPTURE_DIER != 0U || NX_CAPTURE_CR1 != 0U ||
        NX_CAPTURE_PSC != port->config->timer_hz / port->config->tick_hz - 1U ||
        NX_CAPTURE_ARR != NX_CAPTURE_COUNTER_MAX ||
        NX_CAPTURE_CCER != polarity ||
#if NX_CAPTURE_HAS_CR2
        NX_CAPTURE_CR2 != 0U ||
#endif
#if NX_CAPTURE_HAS_SMCR
        NX_CAPTURE_SMCR != 0U ||
#endif
        NX_CAPTURE_CCMR1 !=
            (port->config->channel <= 2U ? capture_mode(port->config) : 0U)
#if NX_CAPTURE_CHANNELS > 2U
        || NX_CAPTURE_CCMR2 !=
               (port->config->channel > 2U ? capture_mode(port->config) : 0U)
#endif
    ) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    port->events = events;
    port->capacity = capacity;
    port->head = port->tail = port->count = 0U;
    port->loss_incidents = port->pending_flags = 0U;
    NX_TIMER_WRITE(NX_CAPTURE_SR, ~(flag | (flag << 8U)));
    NX_TIMER_WRITE(NX_CAPTURE_CCER, NX_CAPTURE_CCER | enable);
    NX_TIMER_WRITE(NX_CAPTURE_CR1, NX_CAPTURE_CR1 | 1U);
    nx_arch_dsb();
    if ((NX_CAPTURE_CCER & enable) == 0U || (NX_CAPTURE_CR1 & 1U) == 0U) {
        NX_TIMER_WRITE(NX_CAPTURE_CCER, NX_CAPTURE_CCER & ~enable);
        NX_TIMER_WRITE(NX_CAPTURE_CR1, NX_CAPTURE_CR1 & ~1U);
        port->events = NULL;
        port->capacity = 0U;
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    port->running = true;
    NX_TIMER_WRITE(NX_CAPTURE_DIER, NX_CAPTURE_DIER | flag);
    nx_arch_dsb();
    if ((NX_CAPTURE_DIER & flag) == 0U) {
        NX_TIMER_WRITE(NX_CAPTURE_CCER, NX_CAPTURE_CCER & ~enable);
        NX_TIMER_WRITE(NX_CAPTURE_CR1, NX_CAPTURE_CR1 & ~1U);
        port->running = false;
        port->events = NULL;
        port->capacity = 0U;
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Retain queued events and the ring until readback proves detachment.
 */
static nx_result_t capture_stop(void* context) {
    nx_timer_capture_controller_t* port = context;
    if (port == NULL || port->config == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!capture_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!NX_CAPTURE_ACCESS_READY()) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    port->draining = port->events != NULL;
    bool cold = !port->initialized;
    const uint32_t enable = 1U << (((uint32_t)port->config->channel - 1U) * 4U);
    const uint32_t flag = 1U << port->config->channel;
    NX_TIMER_WRITE(NX_CAPTURE_DIER, cold ? 0U : NX_CAPTURE_DIER & ~flag);
    NX_TIMER_WRITE(NX_CAPTURE_CCER, cold ? 0U : NX_CAPTURE_CCER & ~enable);
    NX_TIMER_WRITE(NX_CAPTURE_CR1, NX_CAPTURE_CR1 & ~1U);
    nx_arch_dsb();
    if ((NX_CAPTURE_DIER & (cold ? UINT32_MAX : flag)) != 0U ||
        (NX_CAPTURE_CCER & (cold ? UINT32_MAX : enable)) != 0U ||
        (NX_CAPTURE_CR1 & 1U) != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    port->running = false;
    if (port->count != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    port->events = NULL;
    port->capacity = 0U;
    port->draining = false;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Assembly releases static storage only after publisher quiescence. */
nx_result_t NX_CAPTURE_RELEASE(nx_timer_capture_controller_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!capture_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->config == NULL) {
        /* A pure admission refusal owns no peripheral, even if a foreign
         * timer is active. Only exact untouched control can be released. */
        return !port->initialized && port->events == NULL &&
                       port->capacity == 0U && port->head == 0U &&
                       port->tail == 0U && port->count == 0U &&
                       port->loss_incidents == 0U &&
                       port->pending_flags == 0U && !port->running &&
                       !port->draining
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    }
    nx_result_t result = capture_stop(port);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!NX_CAPTURE_VECTOR_QUIET()) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    NX_TIMER_WRITE(NX_CAPTURE_DIER, 0U);
    NX_TIMER_WRITE(NX_CAPTURE_CCER, 0U);
    nx_arch_dsb();
    if (NX_CAPTURE_DIER != 0U || NX_CAPTURE_CCER != 0U ||
        (NX_CAPTURE_CR1 & 1U) != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    *port = (nx_timer_capture_controller_t){0};
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Consume exactly one bounded event; no whole-ring scan is needed. */
static nx_result_t capture_read(void* context,
                                nx_timer_capture_event_t* event) {
    nx_timer_capture_controller_t* port = context;
    if (port == NULL || !port->initialized ||
        !capture_output(port, event, sizeof(*event))) {
        return NX_ERROR_INVALID;
    }
    if (!capture_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (port->count == 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_EMPTY;
    }
    *event = port->events[port->tail];
    ++port->tail;
    if (port->tail == port->capacity) {
        port->tail = 0U;
    }
    --port->count;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Separate snapshot prevents output from overwriting retained state. */
static nx_result_t capture_state(const void* context,
                                 nx_timer_capture_state_t* state) {
    const nx_timer_capture_controller_t* port = context;
    if (port == NULL || !port->initialized ||
        !capture_output(port, state, sizeof(*state))) {
        return NX_ERROR_INVALID;
    }
    if (!capture_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    *state = (nx_timer_capture_state_t){.tick_hz = port->config->tick_hz,
                                        .loss_incidents = port->loss_incidents,
                                        .pending = port->count,
                                        .counter_bits = NX_CAPTURE_COUNTER_BITS,
                                        .running = port->running,
                                        .draining = port->draining};
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Read-clear captured data before observing and acknowledging loss. */
void NX_CAPTURE_IRQ(nx_timer_capture_controller_t* port) {
    if (port == NULL || !port->initialized || !NX_CAPTURE_ACCESS_READY()) {
        return;
    }
    const uint32_t flag = 1U << port->config->channel;
    const uint32_t overcapture = flag << 8U;
    if (port->draining) {
        /* A refused DIER clear can still deliver a late level IRQ. Acknowledge
         * observed facts without writing the retained consumer ring. */
        uint32_t discarded = NX_CAPTURE_SR & (flag | overcapture);
        NX_TIMER_WRITE(NX_CAPTURE_SR, ~discarded);
        if (discarded != 0U) {
            capture_loss(port, NX_TIMER_CAPTURE_RING_LOSS);
        }
        return;
    }
    if (port->events == NULL || !port->running) {
        return;
    }
    uint32_t flags = NX_CAPTURE_SR & (flag | overcapture);
    uint32_t ticks = 0U;
    if ((flags & flag) != 0U) {
        /* Input-mode CCR reads clear CCIF. Preserve any later unread edge;
         * an edge replacing the sampled count must expose its overcapture. */
        ticks = NX_TIMER_READ(*capture_compare(port->config->channel)) &
                NX_CAPTURE_COUNTER_MAX;
        flags |= NX_CAPTURE_SR & overcapture;
    }
    NX_TIMER_WRITE(NX_CAPTURE_SR, ~(flags & overcapture));
    if ((flags & overcapture) != 0U) {
        capture_loss(port, NX_TIMER_CAPTURE_OVERCAPTURE);
    }
    if ((flags & flag) == 0U) {
        return;
    }
    if (port->count == port->capacity) {
        capture_loss(port, NX_TIMER_CAPTURE_RING_LOSS);
        return;
    }
    port->events[port->head] =
        (nx_timer_capture_event_t){.observed_at = nx_time_now_us(),
                                   .captured_ticks = ticks,
                                   .flags = port->pending_flags};
    port->pending_flags = 0U;
    ++port->head;
    if (port->head == port->capacity) {
        port->head = 0U;
    }
    ++port->count;
}
/** \brief One immutable mode table shared by every selected static instance. */
const nx_timer_capture_ops_t NX_CAPTURE_OPS = {.start = capture_start,
                                               .stop = capture_stop,
                                               .read = capture_read,
                                               .state = capture_state};

/* The specialization bindings remain owned by the concrete wrapper. */
#undef NX_CAPTURE_COUNTER_MAX
#ifdef NX_CAPTURE_DEFAULT_TIMER_WRITE
#undef NX_TIMER_WRITE
#undef NX_CAPTURE_DEFAULT_TIMER_WRITE
#endif
#ifdef NX_CAPTURE_DEFAULT_TIMER_READ
#undef NX_TIMER_READ
#undef NX_CAPTURE_DEFAULT_TIMER_READ
#endif
#ifdef NX_CAPTURE_DEFAULT_CAPTURE_COUNTER_BITS
#undef NX_CAPTURE_COUNTER_BITS
#undef NX_CAPTURE_DEFAULT_CAPTURE_COUNTER_BITS
#endif
#ifdef NX_CAPTURE_DEFAULT_CAPTURE_CONTROL_OVERLAP
#undef NX_CAPTURE_CONTROL_OVERLAP
#undef NX_CAPTURE_DEFAULT_CAPTURE_CONTROL_OVERLAP
#endif
#ifdef NX_CAPTURE_DEFAULT_CAPTURE_VECTOR_QUIET
#undef NX_CAPTURE_VECTOR_QUIET
#undef NX_CAPTURE_DEFAULT_CAPTURE_VECTOR_QUIET
#endif
#ifdef NX_CAPTURE_DEFAULT_CAPTURE_ACCESS_READY
#undef NX_CAPTURE_ACCESS_READY
#undef NX_CAPTURE_DEFAULT_CAPTURE_ACCESS_READY
#endif
#ifdef NX_CAPTURE_DEFAULT_CHANNELS
#undef NX_CAPTURE_CHANNELS
#undef NX_CAPTURE_DEFAULT_CHANNELS
#endif
#ifdef NX_CAPTURE_DEFAULT_HAS_CR2
#undef NX_CAPTURE_HAS_CR2
#undef NX_CAPTURE_DEFAULT_HAS_CR2
#endif
#ifdef NX_CAPTURE_DEFAULT_HAS_SMCR
#undef NX_CAPTURE_HAS_SMCR
#undef NX_CAPTURE_DEFAULT_HAS_SMCR
#endif
