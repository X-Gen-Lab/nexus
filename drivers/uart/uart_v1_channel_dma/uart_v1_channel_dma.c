/**
 * \file            uart_v1_channel_dma.c
 * \brief           Legacy USART fixed-channel DMA finite ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "uart_v1_channel_dma.h"
#include "nexus/arch/arch.h"
#include <limits.h>
#include <stddef.h>
#ifndef NX_UART_V1_READ
#define NX_UART_V1_READ(reg)         (*(reg))
#define NX_UART_V1_WRITE(reg, value) (*(reg) = (value))
#endif
#define ENABLE       UINT32_C(1)
#define CHANNEL_IRQS UINT32_C(14)
#define CHANNEL_TC   UINT32_C(2)
#define CHANNEL_TE   UINT32_C(8)
#define UART_TC      (UINT32_C(1) << 6U)
#define UART_IDLE    (UINT32_C(1) << 4U)
#define UART_RXNE    (UINT32_C(1) << 5U)
#define UART_ERRORS  UINT32_C(15)
#define UART_TX_IRQS ((UINT32_C(1) << 7U) | UART_TC)
#define UART_DMAT    (UINT32_C(1) << 7U)
#define UART_DMAR    (UINT32_C(1) << 6U)
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* data, size_t bytes) {
    return data != NULL && bytes != 0U &&
           (uintptr_t)data <= UINTPTR_MAX - bytes;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return as != 0U && bs != 0U && (uintptr_t)a < (uintptr_t)b + bs &&
           (uintptr_t)b < (uintptr_t)a + as;
}
static bool resource_alias(const nx_uart_v1_dma_state_t* s,
                           const nx_uart_v1_dma_config_t* config,
                           const void* data, size_t bytes) {
    return overlap(data, bytes, s, sizeof(*s)) ||
           overlap(data, bytes, config->registers,
                   sizeof(*config->registers)) ||
           overlap(data, bytes, config->tx, sizeof(*config->tx)) ||
           (config->profile == NX_UART_RX_BLOCKS &&
            (overlap(data, bytes, config->receive, sizeof(*config->receive)) ||
             overlap(data, bytes, config->rx, sizeof(*config->rx)))) ||
           overlap(data, bytes, (const void*)config->flags,
                   sizeof(*config->flags)) ||
           overlap(data, bytes, (const void*)config->clear,
                   sizeof(*config->clear)) ||
           overlap(data, bytes, config->regions,
                   config->region_count * sizeof(*config->regions));
}
static bool protected_span(const nx_uart_v1_dma_state_t* s, const void* data,
                           size_t bytes) {
    const nx_uart_v1_dma_config_t* config = s->config;
    return !span(data, bytes) || config == NULL ||
           overlap(data, bytes, config, sizeof(*config)) ||
           resource_alias(s, config, data, bytes) ||
           (config->profile == NX_UART_RX_BYTES &&
            overlap(data, bytes, config->rx_storage, config->rx_capacity)) ||
           (s->wake != NULL && overlap(data, bytes, s->wake, sizeof(*s->wake)));
}
static bool active_alias(const nx_uart_v1_dma_state_t* s, const void* data,
                         size_t bytes) {
    return s->active != NULL &&
           (overlap(data, bytes, s->active, sizeof(*s->active)) ||
            overlap(data, bytes, s->active->data, s->active->length));
}
static bool stream_alias(const nx_uart_v1_dma_state_t* s, const void* data,
                         size_t bytes) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (config->profile != NX_UART_RX_BLOCKS || config->receive == NULL ||
        config->receive->stream == NULL)
        return false;
    if (overlap(data, bytes, config->receive->stream,
                sizeof(*config->receive->stream)) ||
        overlap(data, bytes, config->receive->stream->slots,
                config->receive->stream->count *
                    sizeof(*config->receive->stream->slots)))
        return true;
    for (size_t i = 0U; i < config->receive->stream->count; ++i)
        if (overlap(data, bytes, config->receive->stream->slots[i].data,
                    config->receive->stream->slots[i].capacity))
            return true;
    return false;
}
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_UART_V1_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_UART_V1_READ(reg) & mask) == (value & mask);
}
static uint32_t flags(const nx_uart_v1_dma_state_t* s, bool receive) {
    const nx_uart_v1_dma_config_t* config = s->config;
    return (NX_UART_V1_READ(config->flags) >>
            (receive ? config->rx_shift : config->tx_shift)) &
           UINT32_C(15);
}
static bool acknowledge(nx_uart_v1_dma_state_t* s, bool receive) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (receive && config->profile != NX_UART_RX_BLOCKS)
        return true;
    NX_UART_V1_WRITE(config->clear,
                     UINT32_C(15)
                         << (receive ? config->rx_shift : config->tx_shift));
    nx_arch_dsb();
    return flags(s, receive) == 0U;
}
/** \brief Attempt both source withdrawals even when one refuses. */
static bool detach(nx_uart_v1_dma_state_t* s, bool receive) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (receive && config->profile != NX_UART_RX_BLOCKS)
        return true;
    uint32_t request = receive ? UART_DMAR : UART_DMAT;
    nx_uart_v1_channel_t* channel = receive ? config->rx : config->tx;
    bool peripheral = checked(
        &config->registers->control3,
        NX_UART_V1_READ(&config->registers->control3) & ~request, request);
    bool engine =
        checked(&channel->control,
                NX_UART_V1_READ(&channel->control) & ~(ENABLE | CHANNEL_IRQS),
                ENABLE | CHANNEL_IRQS);
    return peripheral && engine;
}
static void tx_drain(nx_uart_v1_dma_state_t* s, nx_result_t reason,
                     nx_time_us_t now) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (s->terminal == NX_SUCCESS && reason != NX_SUCCESS)
        s->terminal = reason;
    if (!s->tx_draining) {
        s->tx_draining = true;
        s->drain_deadline =
            nx_deadline_after(now, 20000000U / config->baud + 100U);
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    }
    (void)detach(s, false);
}
/** \brief Memory completion never substitutes for the final wire bit. */
static void tx_observe(nx_uart_v1_dma_state_t* s, nx_time_us_t now) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (s->active == NULL || s->wire_complete)
        return;
    uint32_t status = flags(s, false);
    if ((status & CHANNEL_TE) != 0U)
        tx_drain(s, NX_ERROR_IO, now);
    if (!s->tx_draining && (status & CHANNEL_TC) != 0U) {
        if (NX_UART_V1_READ(&config->tx->count) != 0U)
            tx_drain(s, NX_ERROR_IO, now);
        else {
            s->dma_complete = true;
            tx_drain(s, NX_SUCCESS, now);
        }
    }
    if (s->terminal == NX_SUCCESS &&
        nx_deadline_expired(s->active->base.deadline, now))
        tx_drain(s, NX_ERROR_TIMEOUT, now);
    if (!s->tx_draining)
        return;
    bool detached = detach(s, false);
    size_t remaining =
        s->tx_launched ? NX_UART_V1_READ(&config->tx->count) : 0U;
    if (remaining > s->active->length) {
        if (s->terminal == NX_SUCCESS)
            s->terminal = NX_ERROR_IO;
        return;
    }
    bool untouched = !s->tx_launched || (detached && !s->dma_complete &&
                                         remaining == s->active->length);
    if (detached && (untouched || (NX_UART_V1_READ(&config->registers->status) &
                                   UART_TC) != 0U))
        s->wire_complete = true;
    if (!s->wire_complete)
        (void)checked(
            &config->registers->control1,
            (NX_UART_V1_READ(&config->registers->control1) & ~UART_TX_IRQS) |
                UART_TC,
            UART_TX_IRQS);
}
static void tx_service(nx_uart_v1_dma_state_t* s) {
    const nx_uart_v1_dma_config_t* config = s->config;
    nx_uart_tx_request_t* request = s->active;
    if (request == NULL)
        return;
    nx_time_us_t now = nx_time_now_us();
    tx_observe(s, now);
    if (!s->tx_draining)
        return;
    bool detached = detach(s, false);
    bool sources =
        checked(&config->registers->control1,
                NX_UART_V1_READ(&config->registers->control1) & ~UART_TX_IRQS,
                UART_TX_IRQS);
    size_t remaining =
        s->tx_launched ? NX_UART_V1_READ(&config->tx->count) : 0U;
    if (detached && sources && s->wire_complete &&
        remaining <= request->length) {
        if ((flags(s, false) & CHANNEL_TE) != 0U && s->terminal == NX_SUCCESS)
            s->terminal = NX_ERROR_IO;
        if (acknowledge(s, false)) {
            nx_result_t terminal = s->terminal;
            size_t transferred = s->dma_complete && terminal == NX_SUCCESS
                                     ? request->length
                                     : 0U;
            s->active = NULL;
            nx_request_settle(&request->base, terminal, transferred);
            return;
        }
    }
    if (nx_deadline_expired(s->drain_deadline, now))
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
}
static nx_result_t start(void* context, nx_uart_tx_request_t* request,
                         bool admitted) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        protected_span(s, request, sizeof(*request)) ||
        protected_span(s, request->data, request->length) ||
        stream_alias(s, request, sizeof(*request)) ||
        stream_alias(s, request->data, request->length) ||
        overlap(request, sizeof(*request), request->data, request->length))
        return NX_ERROR_INVALID;
    if (request->length > UINT16_MAX)
        return NX_ERROR_UNSUPPORTED;
    nx_result_t result = nx_dma_buffer_validate(
        config->regions, config->region_count, request->data, request->length,
        1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->initialized || s->closing ||
        (NX_UART_V1_READ(&config->registers->control1) &
         ((UINT32_C(1) << 13U) | (UINT32_C(1) << 3U))) !=
            ((UINT32_C(1) << 13U) | (UINT32_C(1) << 3U)))
        result = NX_ERROR_STATE;
    else if (s->active != NULL || (NX_UART_V1_READ(&config->tx->control) &
                                   (ENABLE | CHANNEL_IRQS)) != 0U)
        result = NX_ERROR_BUSY;
    else if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        result = NX_ERROR_TIMEOUT;
    else
        result = admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(guard);
        return result;
    }
    s->active = request;
    s->terminal = NX_SUCCESS;
    s->tx_launched = s->tx_draining = s->dma_complete = s->wire_complete =
        false;
    s->drain_deadline = NX_DEADLINE_NEVER;
    bool valid = acknowledge(s, false);
    valid =
        checked(&config->tx->peripheral,
                (uint32_t)(uintptr_t)&config->registers->data, UINT32_MAX) &&
        valid;
    valid = checked(&config->tx->memory, (uint32_t)(uintptr_t)request->data,
                    UINT32_MAX) &&
            valid;
    valid =
        checked(&config->tx->count, (uint32_t)request->length, UINT16_MAX) &&
        valid;
    uint32_t control = (UINT32_C(1) << 12U) | (UINT32_C(1) << 7U) |
                       (UINT32_C(1) << 4U) | CHANNEL_TC | CHANNEL_TE;
    valid = checked(&config->tx->control, control, UINT32_MAX) && valid;
    valid =
        checked(&config->registers->control1,
                NX_UART_V1_READ(&config->registers->control1) & ~UART_TX_IRQS,
                UART_TX_IRQS) &&
        valid;
    NX_UART_V1_WRITE(&config->registers->status, ~UART_TC);
    nx_arch_dsb();
    valid =
        (NX_UART_V1_READ(&config->registers->status) & UART_TC) == 0U && valid;
    if (valid) {
        s->tx_launched = true;
        valid = checked(&config->tx->control, control | ENABLE, UINT32_MAX);
        if (valid)
            valid = checked(&config->registers->control3,
                            NX_UART_V1_READ(&config->registers->control3) |
                                UART_DMAT,
                            UART_DMAT);
    }
    if (!valid)
        tx_drain(s, NX_ERROR_IO, nx_time_now_us());
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return start(context, request, false);
}
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    return start(context, request, true);
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_uart_v1_dma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (s->active != request)
        result = nx_request_state(&request->base) == NX_REQUEST_SETTLED
                     ? NX_SUCCESS
                     : NX_ERROR_STATE;
    else {
        nx_time_us_t now = nx_time_now_us();
        tx_observe(s, now);
        if (!s->wire_complete)
            tx_drain(s, NX_ERROR_CANCELLED, now);
    }
    nx_arch_irq_restore(guard);
    return result;
}
/** \brief Legacy SR/DR sequence, legal only after RX DMA withdrawal. */
static bool clear_receive(nx_uart_v1_dma_state_t* s) {
    const nx_uart_v1_dma_config_t* config = s->config;
    (void)NX_UART_V1_READ(&config->registers->status);
    (void)NX_UART_V1_READ(&config->registers->data);
    nx_arch_dsb();
    return (NX_UART_V1_READ(&config->registers->status) &
            (UART_IDLE | UART_ERRORS)) == 0U;
}
static bool clear_block_receive(nx_uart_v1_dma_state_t* s) {
    const nx_uart_v1_dma_config_t* config = s->config;
    uint32_t status = NX_UART_V1_READ(&config->registers->status);
    if (config->receive->stream != NULL && (status & UART_RXNE) != 0U)
        nx_stream_note_loss(config->receive->stream, 1U);
    return clear_receive(s);
}
static void rx_observe(nx_uart_v1_dma_state_t* s, uint32_t status) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (!config->receive->filling) {
        if (config->receive->stream != NULL && (status & UART_ERRORS) != 0U) {
            if (!config->receive->gap_error)
                nx_stream_note_loss(config->receive->stream, 1U);
            config->receive->gap_error = true;
            if (clear_block_receive(s))
                config->receive->gap_error = false;
        }
        return;
    }
    uint32_t dma = flags(s, true);
    if ((dma & CHANNEL_TE) != 0U || (status & UART_ERRORS) != 0U ||
        (!config->receive->rx_draining && (dma & CHANNEL_TC) != 0U &&
         NX_UART_V1_READ(&config->rx->count) != 0U))
        config->receive->boundary |= NX_STREAM_BOUNDARY_ERROR;
    if ((status & UART_IDLE) != 0U)
        config->receive->boundary |= NX_STREAM_BOUNDARY_IDLE;
    if ((dma & (CHANNEL_TE | CHANNEL_TC)) != 0U ||
        (status & (UART_IDLE | UART_ERRORS)) != 0U)
        config->receive->rx_draining = true;
    if (config->receive->rx_draining && !detach(s, true))
        config->receive->rx_quarantined = true;
}
/** \brief Validate every block before acquiring the first producer loan. */
static nx_result_t validate_stream(nx_uart_v1_dma_state_t* s,
                                   const nx_stream_t* stream) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (protected_span(s, stream, sizeof(*stream)) ||
        active_alias(s, stream, sizeof(*stream)) || stream->slots == NULL ||
        stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots))
        return NX_ERROR_INVALID;
    size_t metadata = stream->count * sizeof(*stream->slots);
    if (protected_span(s, stream->slots, metadata) ||
        active_alias(s, stream->slots, metadata) ||
        overlap(stream, sizeof(*stream), stream->slots, metadata))
        return NX_ERROR_INVALID;
    if (stream->stopping || stream->outstanding != 0U ||
        stream->producer >= stream->count || stream->consumer >= stream->count)
        return NX_ERROR_STATE;
    if (stream->epoch == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->state != NX_STREAM_SLOT_FREE || slot->capacity > UINT16_MAX ||
            protected_span(s, slot->data, slot->capacity) ||
            active_alias(s, slot->data, slot->capacity) ||
            overlap(slot->data, slot->capacity, stream, sizeof(*stream)) ||
            overlap(slot->data, slot->capacity, stream->slots, metadata))
            return NX_ERROR_INVALID;
        nx_result_t result = nx_dma_buffer_validate(
            config->regions, config->region_count, slot->data, slot->capacity,
            1U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        for (size_t j = 0U; j < i; ++j)
            if (overlap(slot->data, slot->capacity, stream->slots[j].data,
                        stream->slots[j].capacity))
                return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
/** \brief Normal DMA restarts only on a free block, with explicit gaps. */
static void rx_arm(nx_uart_v1_dma_state_t* s) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (config->receive->stream == NULL || config->receive->rx_stopping ||
        config->receive->filling || config->receive->gap_error ||
        !nx_stream_can_reserve(config->receive->stream))
        return;
    if (nx_stream_reserve(config->receive->stream, &config->receive->fill) !=
        NX_SUCCESS)
        return;
    config->receive->filling = true;
    config->receive->rx_launched = config->receive->rx_draining =
        config->receive->rx_quarantined = false;
    config->receive->boundary = 0U;
    bool valid = acknowledge(s, true) && clear_block_receive(s);
    valid =
        checked(&config->rx->peripheral,
                (uint32_t)(uintptr_t)&config->registers->data, UINT32_MAX) &&
        valid;
    valid =
        checked(&config->rx->memory,
                (uint32_t)(uintptr_t)config->receive->fill.data, UINT32_MAX) &&
        valid;
    valid = checked(&config->rx->count,
                    (uint32_t)config->receive->fill.capacity, UINT16_MAX) &&
            valid;
    uint32_t control =
        (UINT32_C(1) << 12U) | (UINT32_C(1) << 7U) | CHANNEL_TC | CHANNEL_TE;
    valid = checked(&config->rx->control, control, UINT32_MAX) && valid;
    if (valid) {
        config->receive->rx_launched = true;
        valid = checked(&config->rx->control, control | ENABLE, UINT32_MAX);
        if (valid)
            valid = checked(&config->registers->control3,
                            NX_UART_V1_READ(&config->registers->control3) |
                                UART_DMAR,
                            UART_DMAR);
    }
    if (!valid) {
        config->receive->boundary |= NX_STREAM_BOUNDARY_ERROR;
        config->receive->rx_draining = true;
        if (!detach(s, true))
            config->receive->rx_quarantined = true;
    }
}
/** \brief Disabled channel and stable count must precede final publication. */
static void rx_finish(nx_uart_v1_dma_state_t* s) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if (!config->receive->filling || !config->receive->rx_draining)
        return;
    if (!detach(s, true)) {
        config->receive->rx_quarantined = true;
        return;
    }
    size_t remaining = config->receive->rx_launched
                           ? NX_UART_V1_READ(&config->rx->count)
                           : config->receive->fill.capacity;
    if (remaining > config->receive->fill.capacity ||
        (flags(s, true) & CHANNEL_TE) != 0U ||
        (NX_UART_V1_READ(&config->registers->status) & UART_ERRORS) != 0U)
        config->receive->boundary |= NX_STREAM_BOUNDARY_ERROR;
    if (!acknowledge(s, true) || !clear_block_receive(s) || !detach(s, true) ||
        (config->receive->rx_launched &&
         NX_UART_V1_READ(&config->rx->count) != remaining)) {
        config->receive->rx_quarantined = true;
        return;
    }
    bool faulty = (config->receive->boundary & NX_STREAM_BOUNDARY_ERROR) != 0U;
    size_t length = remaining <= config->receive->fill.capacity
                        ? config->receive->fill.capacity - remaining
                        : 0U;
    if (faulty)
        nx_stream_note_loss(config->receive->stream, 1U);
    nx_result_t result =
        faulty || length == 0U
            ? nx_stream_abort(config->receive->stream, &config->receive->fill,
                              true)
            : nx_stream_publish(config->receive->stream, &config->receive->fill,
                                length, config->receive->boundary);
    if (result == NX_SUCCESS)
        config->receive->filling = config->receive->rx_launched =
            config->receive->rx_draining = config->receive->rx_quarantined =
                false;
    else
        config->receive->rx_quarantined = true;
}
static void service(void* context) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (s == NULL || !task_context() || !s->initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    tx_service(s);
    rx_observe(s, NX_UART_V1_READ(&config->registers->status));
    rx_finish(s);
    rx_arm(s);
    nx_arch_irq_restore(guard);
}
static void service_tx(void* context) {
    nx_uart_v1_dma_state_t* s = context;
    if (s == NULL || !task_context() || !s->initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    tx_service(s);
    nx_arch_irq_restore(guard);
}
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (config == NULL)
        return NX_ERROR_STATE;
    if (config->profile != NX_UART_RX_BLOCKS)
        return NX_ERROR_UNSUPPORTED;
    if (!s->initialized || s->closing)
        return NX_ERROR_STATE;
    if (config->receive->stream != NULL || config->receive->filling ||
        NX_UART_V1_READ(&config->rx->control) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = validate_stream(s, stream);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    config->receive->stream = stream;
    config->receive->rx_stopping = config->receive->gap_error = false;
    rx_arm(s);
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t rx_stop(void* context) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (config == NULL)
        return NX_ERROR_STATE;
    if (config->profile != NX_UART_RX_BLOCKS)
        return NX_ERROR_UNSUPPORTED;
    if (!s->retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    config->receive->rx_stopping = true;
    config->receive->rx_draining = config->receive->filling;
    nx_result_t result = NX_SUCCESS;
    if (!detach(s, true)) {
        config->receive->rx_quarantined = true;
        result = NX_ERROR_BUSY;
    } else {
        rx_finish(s);
        if (config->receive->filling ||
            !checked(&config->rx->control, 0U, UINT32_MAX))
            result = NX_ERROR_BUSY;
        else if (config->receive->stream != NULL) {
            result = nx_stream_stop(config->receive->stream);
            if (result == NX_SUCCESS)
                config->receive->stream = NULL;
        }
    }
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t read_bytes(void* context, uint8_t* output, size_t capacity,
                              size_t* count) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || protected_span(s, output, capacity) ||
        protected_span(s, count, sizeof(*count)) ||
        active_alias(s, output, capacity) ||
        active_alias(s, count, sizeof(*count)) ||
        overlap(output, capacity, count, sizeof(*count)))
        return NX_ERROR_INVALID;
    if (config->profile != NX_UART_RX_BYTES)
        return NX_ERROR_UNSUPPORTED;
    if (!s->initialized)
        return NX_ERROR_STATE;
    size_t copied = 0U;
    while (copied < capacity) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        if (s->rx_count == 0U) {
            nx_arch_irq_restore(guard);
            break;
        }
        uint8_t byte = ((uint8_t*)config->rx_storage)[s->rx_tail];
        if (++s->rx_tail == config->rx_capacity)
            s->rx_tail = 0U;
        s->rx_count--;
        nx_arch_irq_restore(guard);
        output[copied++] = byte;
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    bool lost = s->rx_loss != 0U;
    s->rx_loss = 0U;
    nx_arch_irq_restore(guard);
    *count = copied;
    return lost ? NX_ERROR_OVERFLOW
                : (copied != 0U ? NX_SUCCESS : NX_ERROR_EMPTY);
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || config == NULL || config->validate_wake == NULL ||
        (wake != NULL && wake != s->wake &&
         (protected_span(s, wake, sizeof(*wake)) ||
          stream_alias(s, wake, sizeof(*wake)) ||
          active_alias(s, wake, sizeof(*wake)))))
        return NX_ERROR_INVALID;
    nx_result_t result = config->validate_wake(s, wake, ceiling);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->initialized || s->closing || s->active != NULL ||
        (config->profile == NX_UART_RX_BLOCKS &&
         config->receive->stream != NULL) ||
        s->wake_publishers != 0U)
        result = NX_ERROR_BUSY;
    else
        s->wake = wake;
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t stop_tx(void* context) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained)
        return NX_SUCCESS;
    s->closing = true;
    if (s->active != NULL) {
        (void)cancel(s, s->active);
        service_tx(s);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    bool busy = s->wake_publishers != 0U;
    nx_arch_irq_restore(guard);
    if (busy || !detach(s, false))
        return NX_ERROR_BUSY;
    if (!checked(&config->registers->control1, 0U, UINT32_MAX) ||
        !checked(&config->registers->control3, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    nx_result_t result = config->stop_irqs(s);
    if (result != NX_SUCCESS)
        return result;
    if (!acknowledge(s, false) ||
        !checked(&config->tx->control, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    guard = nx_arch_irq_save();
    if (s->wake_publishers != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    s->wake = NULL;
    s->initialized = s->retained = false;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s != NULL ? s->config : NULL;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained)
        return NX_SUCCESS;
    s->closing = true;
    nx_result_t result = rx_stop(s);
    if (result != NX_SUCCESS)
        return result;
    if (!acknowledge(s, true) || !checked(&config->rx->control, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    return stop_tx(s);
}
/** \brief Capture the borrowed sink while its publisher owner is retained. */
static void signal(nx_uart_v1_dma_state_t* s) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    const nx_irq_wake_t* wake = s->wake;
    if (wake != NULL && s->wake_publishers != UINT16_MAX)
        s->wake_publishers++;
    else
        wake = NULL;
    nx_arch_irq_restore(guard);
    (void)nx_irq_wake_signal(wake);
    if (wake != NULL) {
        guard = nx_arch_irq_save();
        s->wake_publishers--;
        nx_arch_irq_restore(guard);
    }
}
static void receive_byte(nx_uart_v1_dma_state_t* s, uint32_t status) {
    const nx_uart_v1_dma_config_t* config = s->config;
    if ((status & UART_RXNE) != 0U) {
        uint8_t byte = (uint8_t)NX_UART_V1_READ(&config->registers->data);
        if (s->closing || (status & UART_ERRORS) != 0U ||
            s->rx_count == config->rx_capacity) {
            if (s->rx_loss != UINT32_MAX)
                s->rx_loss++;
        } else {
            ((uint8_t*)config->rx_storage)[s->rx_head] = byte;
            if (++s->rx_head == config->rx_capacity)
                s->rx_head = 0U;
            s->rx_count++;
        }
    } else if ((status & UART_ERRORS) != 0U) {
        if (s->rx_loss != UINT32_MAX)
            s->rx_loss++;
        (void)clear_receive(s);
    }
}
void nx_uart_v1_channel_dma_irq(nx_uart_v1_dma_state_t* s) {
    if (s == NULL || !s->initialized)
        return;
    const nx_uart_v1_dma_config_t* config = s->config;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->initialized) {
        nx_arch_irq_restore(guard);
        return;
    }
    uint32_t status = NX_UART_V1_READ(&config->registers->status);
    tx_observe(s, nx_time_now_us());
    rx_observe(s, status);
    /* Do not steal DATA from a live DMA engine while clearing IDLE. */
    if ((status & (UART_IDLE | UART_ERRORS)) != 0U && detach(s, true))
        (void)clear_block_receive(s);
    nx_arch_irq_restore(guard);
    signal(s);
}
void nx_uart_v1_channel_dma_tx_irq(nx_uart_v1_dma_state_t* s) {
    if (s == NULL || !s->initialized)
        return;
    const nx_uart_v1_dma_config_t* config = s->config;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->initialized) {
        nx_arch_irq_restore(guard);
        return;
    }
    uint32_t status = NX_UART_V1_READ(&config->registers->status);
    tx_observe(s, nx_time_now_us());
    receive_byte(s, status);
    nx_arch_irq_restore(guard);
    signal(s);
}
void nx_uart_v1_channel_dma_tx_channel_irq(nx_uart_v1_dma_state_t* s) {
    if (s == NULL || !s->initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->initialized) {
        nx_arch_irq_restore(guard);
        return;
    }
    tx_observe(s, nx_time_now_us());
    if (s->active == NULL)
        (void)acknowledge(s, false);
    nx_arch_irq_restore(guard);
    signal(s);
}
void nx_uart_v1_channel_dma_channel_irq(nx_uart_v1_dma_state_t* s,
                                        bool receive) {
    if (s == NULL || !s->initialized)
        return;
    const nx_uart_v1_dma_config_t* config = s->config;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->initialized) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (receive && config->profile == NX_UART_RX_BLOCKS)
        rx_observe(s, NX_UART_V1_READ(&config->registers->status));
    else if (!receive)
        tx_observe(s, nx_time_now_us());
    if (receive ? !config->receive->filling : s->active == NULL)
        (void)acknowledge(s, receive);
    nx_arch_irq_restore(guard);
    signal(s);
}
bool nx_uart_v1_channel_dma_cold(const nx_uart_v1_dma_state_t* s) {
    if (s == NULL || s->retained || s->initialized || s->active != NULL ||
        s->wake != NULL || s->wake_publishers != 0U)
        return false;
    const nx_uart_v1_dma_config_t* config = s->config;
    return config == NULL || config->profile != NX_UART_RX_BLOCKS ||
           (config->receive->stream == NULL && !config->receive->filling);
}
static nx_result_t initialize(nx_uart_v1_dma_state_t* s,
                              const nx_uart_v1_dma_config_t* config,
                              uint32_t clock_hz) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !span(config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (!nx_uart_v1_channel_dma_cold(s))
        return NX_ERROR_BUSY;
    if (config->registers == NULL || config->tx == NULL ||
        config->flags == NULL || config->clear == NULL ||
        config->regions == NULL || config->region_count == 0U ||
        config->region_count > SIZE_MAX / sizeof(*config->regions) ||
        !span(config->regions,
              config->region_count * sizeof(*config->regions)) ||
        config->stop_irqs == NULL || config->validate_wake == NULL ||
        config->baud == 0U || config->tx_shift > 24U ||
        config->rx_shift > 24U || config->tx_shift % 4U != 0U ||
        config->rx_shift % 4U != 0U || config->tx_shift == config->rx_shift)
        return NX_ERROR_INVALID;
    if (clock_hz != 8000000U || (config->profile != NX_UART_RX_BYTES &&
                                 config->profile != NX_UART_RX_BLOCKS))
        return NX_ERROR_UNSUPPORTED;
    if (config->profile == NX_UART_RX_BLOCKS) {
        if (!span(s, sizeof(nx_uart_v1_dma_duplex_state_t)) ||
            config->receive !=
                (nx_uart_v1_dma_receive_t*)((uintptr_t)s +
                                            offsetof(
                                                nx_uart_v1_dma_duplex_state_t,
                                                receive)) ||
            config->rx == NULL || config->rx == config->tx ||
            config->rx_capacity != 0U)
            return NX_ERROR_INVALID;
        if (config->receive->stream != NULL || config->receive->filling ||
            NX_UART_V1_READ(&config->rx->control) != 0U)
            return NX_ERROR_BUSY;
    } else if (config->rx != NULL ||
               !span(config->rx_storage, config->rx_capacity) ||
               overlap(config->rx_storage, config->rx_capacity, config,
                       sizeof(*config)) ||
               resource_alias(s, config, config->rx_storage,
                              config->rx_capacity))
        return NX_ERROR_INVALID;
    if (resource_alias(s, config, config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (NX_UART_V1_READ(&config->registers->control1) != 0U ||
        NX_UART_V1_READ(&config->registers->control3) != 0U ||
        NX_UART_V1_READ(&config->tx->control) != 0U)
        return NX_ERROR_BUSY;
    uint32_t divisor = (clock_hz + config->baud / 2U) / config->baud;
    if (divisor < 16U || divisor > UINT16_MAX)
        return NX_ERROR_UNSUPPORTED;
    const nx_uart_v1_dma_config_t* previous = s->config;
    s->config = config;
    nx_result_t result =
        config->validate_wake(s, NULL, config->irq_policy.syscall_ceiling);
    if (result != NX_SUCCESS) {
        s->config = previous;
        return result;
    }
    /* This exact descriptor remains bound through every failed withdrawal. */
    s->retained = true;
    if (config->profile == NX_UART_RX_BLOCKS) {
        config->receive->rx_launched = config->receive->rx_draining = false;
        config->receive->rx_stopping = config->receive->rx_quarantined =
            config->receive->gap_error = false;
    }
    s->closing = s->tx_launched = s->tx_draining = s->dma_complete = false;
    s->wire_complete = false;
    s->rx_head = s->rx_tail = s->rx_count = 0U;
    s->rx_loss = 0U;
    s->wake_publishers = 0U;
    bool valid = acknowledge(s, false) && acknowledge(s, true);
    valid = checked(&config->registers->control2, 0U, UINT32_MAX) && valid;
    valid = checked(&config->registers->control3, 1U, UINT32_MAX) && valid;
    valid = checked(&config->registers->baud, divisor, UINT32_MAX) && valid;
    valid = clear_receive(s) && valid;
    uint32_t control =
        (UINT32_C(1) << 13U) | (UINT32_C(1) << 3U) | (UINT32_C(1) << 2U) |
        (UINT32_C(1) << 8U) |
        (config->profile == NX_UART_RX_BYTES ? UART_RXNE : UART_IDLE);
    valid = checked(&config->registers->control1, control, UINT32_MAX) && valid;
    if (!valid) {
        s->closing = true;
        return NX_ERROR_IO;
    }
    s->initialized = true;
    return NX_SUCCESS;
}
nx_result_t
nx_uart_v1_channel_dma_initialize(nx_uart_v1_dma_state_t* s,
                                  const nx_uart_v1_dma_config_t* config,
                                  uint32_t clock_hz) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || config == NULL || config->profile != NX_UART_RX_BLOCKS)
        return NX_ERROR_INVALID;
    if (!nx_uart_v1_channel_dma_cold(s))
        return NX_ERROR_BUSY;
    nx_result_t result = initialize(s, config, clock_hz);
    if (result != NX_SUCCESS && s->retained)
        (void)stop(s);
    return result;
}
const nx_uart_ops_t nx_uart_v1_channel_dma_ops = {
    submit, start_admitted, cancel,      service,  NULL,
    NULL,   stop,           attach_wake, rx_start, rx_stop};
nx_result_t
nx_uart_v1_channel_dma_tx_initialize(nx_uart_v1_dma_state_t* s,
                                     const nx_uart_v1_dma_config_t* config,
                                     uint32_t clock_hz) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || config == NULL || config->profile != NX_UART_RX_BYTES)
        return NX_ERROR_INVALID;
    if (!nx_uart_v1_channel_dma_cold(s))
        return NX_ERROR_BUSY;
    nx_result_t result = initialize(s, config, clock_hz);
    if (result != NX_SUCCESS && s->retained)
        (void)stop_tx(s);
    return result;
}
const nx_uart_ops_t nx_uart_v1_channel_dma_tx_ops = {
    submit,     start_admitted, cancel,      service_tx, NULL,
    read_bytes, stop_tx,        attach_wake, NULL,       NULL};
