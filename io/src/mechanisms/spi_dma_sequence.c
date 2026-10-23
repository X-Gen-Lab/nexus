/**
 * \file            spi_dma_sequence.c
 * \brief           Optional retained whole-CS DMA segment execution owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "spi_dma_sequence.h"
#include "nexus/arch/arch.h"

/** \brief Accept only a complete bounded hardware boundary contract. */
static bool methods_valid(const nx_spi_dma_sequence_backend_t* backend,
                          const void* context) {
    return backend != NULL && context != NULL && backend->validate != NULL &&
           backend->begin != NULL && backend->advance != NULL &&
           backend->cancel != NULL && backend->release != NULL &&
           backend->stop != NULL;
}
/** \brief Check selected controller metadata without hardware access. */
static bool backend_valid(const nx_spi_dma_sequence_state_t* state) {
    return state != NULL && methods_valid(state->backend, state->context);
}
/** \brief Reopening an owner never clears active or quarantined storage. */
nx_result_t
nx_spi_dma_sequence_initialize(nx_spi_dma_sequence_state_t* state,
                               const nx_spi_dma_sequence_backend_t* backend,
                               void* context) {
    if (state == NULL ||
        (uintptr_t)state % _Alignof(nx_spi_dma_sequence_state_t) != 0U ||
        sizeof(*state) > UINTPTR_MAX - (uintptr_t)state ||
        !methods_valid(backend, context) ||
        sizeof(*backend) > UINTPTR_MAX - (uintptr_t)backend) {
        return NX_ERROR_INVALID;
    }
    if ((state->backend != NULL && state->backend != backend) ||
        (state->context != NULL && state->context != context)) {
        return NX_ERROR_INVALID;
    }
    uintptr_t begin = (uintptr_t)state;
    uintptr_t end = begin + sizeof(*state);
    if (((uintptr_t)backend < end &&
         begin < (uintptr_t)backend + sizeof(*backend)) ||
        ((uintptr_t)context >= begin && (uintptr_t)context < end)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_request_state_t phase = nx_request_state(&state->phase_request.base);
    if (state->active != NULL || state->cs_held || state->pending_release ||
        (state->stopping && !state->stopped) ||
        (phase != NX_REQUEST_READY && phase != NX_REQUEST_SETTLED)) {
        return NX_ERROR_BUSY;
    }
    *state =
        (nx_spi_dma_sequence_state_t){.backend = backend, .context = context};
    nx_request_initialize(&state->phase_request.base);
    return NX_SUCCESS;
}
/** \brief Release all provider references before final outer publication. */
static void finish(nx_spi_dma_sequence_state_t* state) {
    if (state->cs_held) {
        nx_result_t result =
            state->backend->release(state->context, state->endpoint);
        if (result != NX_SUCCESS) {
            state->pending_release = true;
            if (state->terminal == NX_SUCCESS) {
                state->terminal = result;
            }
            if (nx_request_state(&state->active->base) !=
                NX_REQUEST_QUARANTINED) {
                (void)nx_request_transition(&state->active->base,
                                            NX_REQUEST_QUARANTINED);
            }
            return;
        }
    }
    nx_spi_dma_transaction_request_t* request = state->active;
    nx_result_t result = state->terminal;
    size_t completed = state->completed;
    state->active = NULL;
    state->endpoint = NULL;
    state->cs_held = false;
    state->pending_release = false;
    /* The embedded phase also stops referencing caller payload/descriptors. */
    state->phase_request.tx = NULL;
    state->phase_request.rx = NULL;
    state->phase_request.length = 0U;
    nx_request_settle(&request->base, result, completed);
}
/** \brief One phase descriptor borrows words while the outer loan remains. */
static nx_result_t begin_phase(nx_spi_dma_sequence_state_t* state) {
    const nx_spi_dma_transaction_request_t* request = state->active;
    const nx_spi_dma_segment_t* segment = &request->segments[state->phase];
    nx_result_t result =
        nx_request_prepare(&state->phase_request.base, request->base.deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->phase_request.tx = segment->tx;
    state->phase_request.rx = segment->rx;
    state->phase_request.length = segment->words;
    result = state->backend->begin(state->context, state->endpoint,
                                   &state->phase_request, request->word_bits,
                                   segment->memory_flags, state->cs_held);
    if (result == NX_SUCCESS) {
        state->cs_held = true;
    }
    return result;
}
/** \brief Validate all static and hardware constraints before acquiring loans.
 */
static nx_result_t submit(void* context,
                          nx_spi_dma_transaction_request_t* request) {
    const nx_spi_dma_sequence_endpoint_t* endpoint = context;
    if (endpoint == NULL || endpoint->endpoint == NULL ||
        !backend_valid(endpoint->controller) || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_spi_dma_sequence_state_t* state = endpoint->controller;
    if (state->stopping) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_spi_dma_transaction_validate(request);
    if (result != NX_SUCCESS) {
        return result;
    }
    result =
        nx_spi_dma_sequence_storage_validate(request, state, sizeof(*state));
    if (result == NX_SUCCESS) {
        result = nx_spi_dma_sequence_storage_validate(request, endpoint,
                                                      sizeof(*endpoint));
    }
    if (result == NX_SUCCESS) {
        result = nx_spi_dma_sequence_storage_validate(request, state->backend,
                                                      sizeof(*state->backend));
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    if (nx_request_state(&request->base) != NX_REQUEST_READY) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    result =
        state->backend->validate(state->context, endpoint->endpoint, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->active = request;
    state->endpoint = endpoint->endpoint;
    state->phase = 0U;
    state->completed = 0U;
    state->terminal = NX_SUCCESS;
    state->cancelled = false;
    state->cs_held = false;
    state->pending_release = false;
    nx_request_initialize(&state->phase_request.base);
    result = begin_phase(state);
    if (result != NX_SUCCESS) {
        /* Admission has already succeeded: publish a failed accepted request.
         */
        state->terminal = result;
        finish(state);
    }
    return NX_SUCCESS;
}
/** \brief Cancellation targets only the currently retained outer request. */
static nx_result_t cancel(void* context,
                          nx_spi_dma_transaction_request_t* request) {
    nx_spi_dma_sequence_state_t* state = context;
    if (!backend_valid(state) || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->active != request) {
        return nx_request_state(&request->base) == NX_REQUEST_SETTLED
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    }
    if (state->cancelled || state->pending_release) {
        return NX_SUCCESS;
    }
    nx_result_t result =
        state->backend->cancel(state->context, &state->phase_request);
    if (result == NX_SUCCESS) {
        state->cancelled = true;
        if (nx_request_state(&request->base) == NX_REQUEST_ACTIVE) {
            (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
        }
    }
    return result;
}
/** \brief A service call advances at most one completed segment boundary. */
static void service(void* context) {
    nx_spi_dma_sequence_state_t* state = context;
    if (!backend_valid(state) || state->active == NULL || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return;
    }
    if (state->pending_release) {
        finish(state);
        return;
    }
    state->backend->advance(state->context);
    nx_request_state_t phase = nx_request_state(&state->phase_request.base);
    if (phase == NX_REQUEST_QUARANTINED || phase == NX_REQUEST_DRAINING) {
        nx_request_state_t outer = nx_request_state(&state->active->base);
        if (outer != phase && outer != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&state->active->base, phase);
        }
        return;
    }
    if (phase != NX_REQUEST_SETTLED) {
        return;
    }
    state->completed += state->phase_request.base.transferred;
    nx_result_t result = state->phase_request.base.result;
    if (result != NX_SUCCESS || state->cancelled ||
        state->phase + 1U == state->active->count) {
        state->terminal = result != NX_SUCCESS ? result
                          : state->cancelled   ? NX_ERROR_CANCELLED
                                               : NX_SUCCESS;
        finish(state);
        return;
    }
    ++state->phase;
    result = begin_phase(state);
    if (result != NX_SUCCESS) {
        state->terminal = result;
        finish(state);
    }
}
/** \brief Closed admission never revokes a retained or quarantined request. */
static nx_result_t stop(void* context) {
    nx_spi_dma_sequence_state_t* state = context;
    if (!backend_valid(state)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->stopped) {
        return NX_SUCCESS;
    }
    state->stopping = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    nx_result_t result = state->backend->stop(state->context);
    if (result == NX_SUCCESS) {
        /* Optional source observers share this cold handoff publication guard.
         */
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        state->stopped = true;
        nx_arch_irq_restore(saved);
    }
    return result;
}
/** \brief Shared immutable endpoint methods add no per-request method table. */
const nx_spi_dma_transaction_endpoint_ops_t nx_spi_dma_sequence_endpoint_ops = {
    .submit = submit,
};
/** \brief Optional sequence controllers alone select these operation methods.
 */
const nx_spi_dma_transaction_port_ops_t nx_spi_dma_sequence_port_ops = {
    .cancel = cancel,
    .service = service,
    .stop = stop,
};
