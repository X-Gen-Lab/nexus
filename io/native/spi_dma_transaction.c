/**
 * \file            spi_dma_transaction.c
 * \brief           Real Native word model for the common segmented DMA owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/native/spi_dma_model.h"
#include "provider.h"
#include "spi_dma_provider.h"

#ifndef NX_NATIVE_SPI_DMA_BEFORE_STEP
#define NX_NATIVE_SPI_DMA_BEFORE_STEP() ((void)0)
#endif
#ifndef NX_NATIVE_SPI_DMA_AFTER_ACTIVE
#define NX_NATIVE_SPI_DMA_AFTER_ACTIVE() ((void)0)
#endif

/** \brief Exact typed owner identity prevents old stopped handles from driving.
 */
static nx_native_spi_dma_state_t*
model_state(const nx_spi_dma_transaction_port_t* port) {
    if (port == NULL || port->ops != &nx_spi_dma_sequence_port_ops ||
        port->context == NULL) {
        return NULL;
    }
    const nx_spi_dma_sequence_state_t* sequence = port->context;
    if (sequence->backend != &nx_native_spi_dma_sequence_backend ||
        sequence->stopped || sequence->context == NULL) {
        return NULL;
    }
    return sequence->context;
}
nx_result_t nx_native_spi_dma_initialize(nx_native_spi_dma_state_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->active != NULL || state->selected != NULL) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    if (state->opened) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    *state = (nx_native_spi_dma_state_t){.fail_after = SIZE_MAX,
                                         .opened = true,
                                         .automatic_irq = true,
                                         .wire_idle = true};
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Every future segment rejects before the first model CS or borrow. */
static nx_result_t validate(void* context, void* endpoint,
                            const nx_spi_dma_transaction_request_t* request) {
    nx_native_spi_dma_state_t* state = context;
    const nx_native_spi_dma_endpoint_state_t* device = endpoint;
    if (state == NULL || device == NULL || device->controller != state ||
        request == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool available = state->opened && !state->stopping;
    bool busy = state->active != NULL || state->selected != NULL || device->cs;
    nx_arch_irq_restore(saved);
    if (!available) {
        return NX_ERROR_STATE;
    }
    if (busy) {
        return NX_ERROR_BUSY;
    }
    /* Immutable span checks do not extend the IRQ metadata exclusion. */
    nx_result_t result =
        nx_spi_dma_sequence_storage_validate(request, state, sizeof(*state));
    if (result == NX_SUCCESS) {
        result = nx_spi_dma_sequence_storage_validate(request, device,
                                                      sizeof(*device));
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    for (size_t i = 0U; i < request->count; ++i) {
        if (request->segments[i].words > UINT16_MAX) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    return NX_SUCCESS;
}
/** \brief Phase length carries words privately; no legacy byte face is used. */
static nx_result_t begin(void* context, void* endpoint, nx_spi_request_t* phase,
                         uint8_t bits, uint8_t flags, bool retained_cs) {
    nx_native_spi_dma_state_t* state = context;
    nx_native_spi_dma_endpoint_state_t* device = endpoint;
    if (state == NULL || device == NULL || device->controller != state ||
        phase == NULL || (bits != 8U && bits != 16U)) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(phase->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->opened || state->stopping) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (state->active != NULL ||
        (retained_cs ? state->selected != device || !device->cs
                     : state->selected != NULL || device->cs)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_request_admit(&phase->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    /* Source observers cannot see an active phase before format and CS. */
    state->active = phase;
    NX_NATIVE_SPI_DMA_AFTER_ACTIVE();
    state->selected = device;
    state->shifted = 0U;
    state->word_bits = bits;
    state->memory_flags = flags;
    state->terminal = false;
    state->terminal_result = NX_SUCCESS;
    state->wire_idle = false;
    if (!retained_cs && device->assertions != SIZE_MAX) {
        ++device->assertions;
    }
    device->cs = true;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Model one complete word or independent idle proof, then publish hint.
 */
static void step(nx_native_spi_dma_state_t* state,
                 const nx_spi_dma_transaction_port_t* expected) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (expected != NULL && model_state(expected) != state) {
        nx_arch_irq_restore(saved);
        return;
    }
    nx_spi_request_t* phase = state->active;
    const nx_irq_wake_t* wake = state->wake;
    bool notify = false;
    if (!state->opened || phase == NULL) {
        nx_arch_irq_restore(saved);
        return;
    }
    if (state->terminal) {
        if (!state->wire_idle && !state->hold_drain) {
            state->wire_idle = true;
            notify = true;
        }
    } else if (nx_deadline_expired(phase->base.deadline, nx_time_now_us()) ||
               state->shifted == state->fail_after ||
               state->shifted == phase->length) {
        state->terminal_result =
            nx_deadline_expired(phase->base.deadline, nx_time_now_us())
                ? NX_ERROR_TIMEOUT
            : state->shifted == state->fail_after ? NX_ERROR_IO
                                                  : NX_SUCCESS;
        state->terminal = true;
        state->wire_idle = !state->hold_drain;
        notify = true;
    } else {
        size_t tx = (state->memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U
                        ? 0U
                        : state->shifted;
        size_t rx = (state->memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U
                        ? 0U
                        : state->shifted;
        if (state->word_bits == 16U) {
            ((uint16_t*)phase->rx)[rx] = ((const uint16_t*)phase->tx)[tx];
        } else {
            phase->rx[rx] = phase->tx[tx];
        }
        ++state->shifted;
    }
    nx_arch_irq_restore(saved);
    if (notify) {
        (void)nx_irq_wake_signal(wake);
    }
}
void nx_native_spi_dma_model_step(const nx_spi_dma_transaction_port_t* port) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_native_spi_dma_state_t* state = model_state(port);
    nx_arch_irq_restore(saved);
    if (state != NULL) {
        NX_NATIVE_SPI_DMA_BEFORE_STEP();
        /* Recheck after a queued source could have crossed owner handoff. */
        step(state, port);
    }
}
/** \brief Cancellation latches intent; it does not manufacture wire idle. */
static nx_result_t cancel(void* context, nx_spi_request_t* phase) {
    nx_native_spi_dma_state_t* state = context;
    if (state == NULL || phase == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->active != phase) {
        nx_result_t result =
            nx_request_state(&phase->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(saved);
        return result;
    }
    if (!state->terminal) {
        state->terminal = true;
        state->terminal_result = NX_ERROR_CANCELLED;
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Separate model progress from acquire-visible phase settlement. */
static void advance(void* context) {
    nx_native_spi_dma_state_t* state = context;
    if (state == NULL) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->opened || state->active == NULL) {
        nx_arch_irq_restore(saved);
        return;
    }
    if (!state->terminal &&
        nx_deadline_expired(state->active->base.deadline, nx_time_now_us())) {
        state->terminal = true;
        state->terminal_result = NX_ERROR_TIMEOUT;
    }
    bool automatic = state->automatic_irq;
    nx_arch_irq_restore(saved);
    if (automatic) {
        step(state, NULL);
    }
    saved = nx_arch_irq_save();
    nx_spi_request_t* phase = state->active;
    if (phase == NULL || !state->terminal) {
        nx_arch_irq_restore(saved);
        return;
    }
    if (state->hold_drain || !state->wire_idle) {
        (void)nx_request_transition(&phase->base, state->hold_drain
                                                      ? NX_REQUEST_QUARANTINED
                                                      : NX_REQUEST_DRAINING);
        nx_arch_irq_restore(saved);
        return;
    }
    nx_result_t result = state->terminal_result;
    size_t words = result == NX_SUCCESS ? phase->length : 0U;
    state->active = NULL;
    state->terminal = false;
    nx_arch_irq_restore(saved);
    /* Every source now observes NULL before final phase publication. */
    nx_request_settle(&phase->base, result, words);
}
/** \brief Only final outer drain releases the selected CS identity. */
static nx_result_t release(void* context, void* endpoint) {
    nx_native_spi_dma_state_t* state = context;
    nx_native_spi_dma_endpoint_state_t* device = endpoint;
    if (state == NULL || device == NULL || device->controller != state) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->selected != device || !device->cs) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_INVALID;
    }
    if (state->active != NULL || !state->wire_idle || state->hold_drain) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    device->cs = false;
    if (device->releases != SIZE_MAX) {
        ++device->releases;
    }
    state->selected = NULL;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Unopened or successfully stopped storage has no acquired publisher.
 */
static nx_result_t stop(void* context) {
    nx_native_spi_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->opened) {
        nx_arch_irq_restore(saved);
        return NX_SUCCESS;
    }
    state->stopping = true;
    nx_spi_request_t* phase = state->active;
    nx_arch_irq_restore(saved);
    if (phase != NULL) {
        (void)cancel(state, phase);
        advance(state);
    }
    saved = nx_arch_irq_save();
    if (state->active != NULL || state->selected != NULL || !state->wire_idle ||
        state->hold_drain) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    state->opened = false;
    state->wake = NULL;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
const nx_spi_dma_sequence_backend_t nx_native_spi_dma_sequence_backend = {
    validate, begin, advance, cancel, release, stop};

/** \brief A logical child cannot claim a foreign physical Native endpoint. */
bool nx_native_spi_dma_owner_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port) {
    if (!nx_spi_dma_sequence_contains(endpoint, port)) {
        return false;
    }
    const nx_spi_dma_sequence_state_t* sequence = port->context;
    const nx_spi_dma_sequence_endpoint_t* child = endpoint->context;
    if (sequence->backend != &nx_native_spi_dma_sequence_backend ||
        sequence->context == NULL) {
        return false;
    }
    const nx_native_spi_dma_endpoint_state_t* device = child->endpoint;
    return device->controller == sequence->context;
}
nx_result_t
nx_native_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling) {
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 240U, 4U, 5U};
    const nx_irq_source_t source = {0, 5U, 0U};
    nx_result_t result =
        nx_native_irq_wake_validate(wake, &policy, &source, syscall_ceiling);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_native_spi_dma_state_t* state = model_state(port);
    if (state == NULL || !state->opened || state->stopping) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    state->wake = wake;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
nx_result_t
nx_native_spi_dma_model_automatic(const nx_spi_dma_transaction_port_t* port,
                                  bool automatic) {
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_native_spi_dma_state_t* state = model_state(port);
    if (state == NULL || !state->opened) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (state->active != NULL || state->selected != NULL) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    state->automatic_irq = automatic;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
nx_result_t
nx_native_spi_dma_model_hold(const nx_spi_dma_transaction_port_t* port,
                             bool hold) {
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_native_spi_dma_state_t* state = model_state(port);
    if (state == NULL || !state->opened) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    state->hold_drain = hold;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
nx_result_t
nx_native_spi_dma_model_fault(const nx_spi_dma_transaction_port_t* port,
                              size_t fail_after) {
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_native_spi_dma_state_t* state = model_state(port);
    if (state == NULL || !state->opened) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    state->fail_after = fail_after;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
