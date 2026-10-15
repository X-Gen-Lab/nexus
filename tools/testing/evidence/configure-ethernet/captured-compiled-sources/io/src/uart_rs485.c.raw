/**
 * \file            uart_rs485.c
 * \brief           Selected RS485 setup, transport drain and direction hold
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/uart_rs485.h"
#include "nexus/arch/arch.h"

#define PHASE_SETUP  1U
#define PHASE_ACTIVE 2U
#define PHASE_HOLD   3U

/** \brief The one serialized task owns adapter and underlying transport. */
static nx_result_t control(const nx_uart_rs485_state_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    return state->initialized ? NX_SUCCESS : NX_ERROR_STATE;
}

/** \brief One authorized output store implements the declared polarity. */
static nx_result_t direction(const nx_uart_rs485_state_t* state, bool active) {
    const nx_uart_rs485_config_t* config = state->config;
    bool high = active == config->active_high;
    return nx_gpio_port_write(config->direction, high ? config->mask : 0U,
                              high ? 0U : config->mask);
}

/** \brief Reject an overlap with adapter control before payload is borrowed. */
static bool overlaps_state(const nx_uart_rs485_state_t* state,
                           const void* pointer, size_t length) {
    uintptr_t first = (uintptr_t)pointer;
    uintptr_t second = (uintptr_t)state;
    return length > UINTPTR_MAX - first ||
           sizeof(*state) > UINTPTR_MAX - second ||
           (first < second + sizeof(*state) && second < first + length);
}

/** \brief Validate and bind immutable facts before changing the DE output. */
nx_result_t nx_uart_rs485_initialize(nx_uart_rs485_state_t* state,
                                     const nx_uart_rs485_config_t* config) {
    if (state == NULL || config == NULL ||
        overlaps_state(state, config, sizeof(*config))) {
        return NX_ERROR_INVALID;
    }
    if (config->transport == NULL ||
        overlaps_state(state, config->transport, sizeof(*config->transport)) ||
        config->direction == NULL ||
        overlaps_state(state, config->direction, sizeof(*config->direction)) ||
        config->transport->ops == NULL || config->transport->context == NULL ||
        config->direction == NULL || config->direction->ops == NULL ||
        config->direction->context == NULL || config->validate == NULL ||
        config->mask == 0U || (config->mask & (config->mask - 1U)) != 0U ||
        config->setup_us > UINT32_MAX || config->hold_us > UINT32_MAX ||
        overlaps_state(state, config->transport->context, 1U) ||
        overlaps_state(state, config->direction->context, 1U) ||
        config->transport->ops->start_admitted == NULL ||
        config->transport->ops->cancel == NULL ||
        config->transport->ops->service == NULL ||
        config->transport->ops->stop == NULL ||
        config->direction->ops->write == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->initialized) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_gpio_port_write(
        config->direction, config->active_high ? 0U : config->mask,
        config->active_high ? config->mask : 0U);
    if (result != NX_SUCCESS) {
        return result;
    }
    *state = (nx_uart_rs485_state_t){.config = config, .initialized = true};
    nx_request_initialize(&state->inner.base);
    return NX_SUCCESS;
}

/** \brief Validate the same transport identity before asserting direction. */
static nx_result_t validate(nx_uart_rs485_state_t* state,
                            const nx_uart_tx_request_t* request,
                            nx_request_state_t required) {
    nx_result_t result = control(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (request == NULL || request->data == NULL || request->length == 0U ||
        overlaps_state(state, request, sizeof(*request)) ||
        overlaps_state(state, request->data, request->length)) {
        return NX_ERROR_INVALID;
    }
    if (state->closing) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (nx_request_state(&request->base) != required) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return state->config->validate(state->config->transport->context, request);
}

/** \brief Accepted direction failure still retains a cleanup-owned borrow. */
static void begin(nx_uart_rs485_state_t* state, nx_uart_tx_request_t* request) {
    state->active = request;
    state->transferred = 0U;
    state->terminal = direction(state, true);
    nx_time_us_t now = nx_time_now_us();
    state->phase = state->terminal == NX_SUCCESS ? PHASE_SETUP : PHASE_HOLD;
    state->boundary = nx_deadline_after(
        now, state->terminal == NX_SUCCESS ? state->config->setup_us : 0U);
}

/** \brief No GPIO or transport effect occurs until unique admission succeeds.
 */
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = validate(state, request, NX_REQUEST_READY);
    if (result == NX_SUCCESS) {
        result = nx_request_admit(&request->base, NX_REQUEST_QUEUED);
        if (result == NX_SUCCESS) {
            begin(state, request);
        }
    }
    return result;
}

/** \brief Preserve the outer adapter admission through setup and transport. */
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = validate(state, request, NX_REQUEST_QUEUED);
    if (result == NX_SUCCESS) {
        begin(state, request);
    }
    return result;
}

/** \brief Drop every internal payload reference before outer final publish. */
static void begin_hold(nx_uart_rs485_state_t* state, nx_result_t terminal,
                       size_t transferred, nx_time_us_t duration) {
    state->terminal = terminal;
    state->transferred = transferred;
    state->inner.data = NULL;
    state->inner.length = 0U;
    state->phase = PHASE_HOLD;
    state->boundary = nx_deadline_after(nx_time_now_us(), duration);
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
}

/** \brief Each call makes one bounded transport or GPIO progress attempt. */
static void service(void* context) {
    nx_uart_rs485_state_t* state = context;
    if (control(state) != NX_SUCCESS || state->active == NULL) {
        return;
    }
    if (state->phase == PHASE_SETUP) {
        if (nx_deadline_expired(state->active->base.deadline,
                                nx_time_now_us())) {
            if (nx_request_state(&state->inner.base) == NX_REQUEST_QUEUED) {
                nx_request_settle(&state->inner.base, NX_ERROR_TIMEOUT, 0U);
            }
            begin_hold(state, NX_ERROR_TIMEOUT, 0U, 0U);
        } else if (nx_deadline_expired(state->boundary, nx_time_now_us())) {
            nx_result_t result = NX_SUCCESS;
            if (nx_request_state(&state->inner.base) != NX_REQUEST_QUEUED) {
                result = nx_uart_tx_prepare(&state->inner, state->active->data,
                                            state->active->length,
                                            state->active->base.deadline);
                if (result == NX_SUCCESS) {
                    result =
                        nx_request_admit(&state->inner.base, NX_REQUEST_QUEUED);
                }
            }
            if (result == NX_SUCCESS) {
                result = nx_uart_port_start_admitted(state->config->transport,
                                                     &state->inner);
            }
            if (result == NX_SUCCESS) {
                state->phase = PHASE_ACTIVE;
                (void)nx_request_transition(&state->active->base,
                                            NX_REQUEST_ACTIVE);
            } else if (result != NX_ERROR_BUSY) {
                nx_request_settle(&state->inner.base, result, 0U);
                begin_hold(state, result, 0U, 0U);
            }
        }
    }
    if (state->phase == PHASE_ACTIVE) {
        nx_uart_port_service(state->config->transport);
        nx_request_state_t observed = nx_request_state(&state->inner.base);
        if (observed == NX_REQUEST_SETTLED) {
            nx_result_t result = state->inner.base.result;
            size_t transferred = state->inner.base.transferred;
            begin_hold(state, result, transferred, state->config->hold_us);
        } else if (observed == NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&state->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
    }
    if (state->phase == PHASE_HOLD &&
        nx_deadline_expired(state->boundary, nx_time_now_us())) {
        nx_result_t result = direction(state, false);
        if (result != NX_SUCCESS) {
            (void)nx_request_transition(&state->active->base,
                                        NX_REQUEST_DRAINING);
            (void)nx_request_transition(&state->active->base,
                                        NX_REQUEST_QUARANTINED);
            return;
        }
        nx_uart_tx_request_t* request = state->active;
        nx_result_t terminal = state->terminal;
        size_t transferred = state->transferred;
        state->active = NULL;
        state->phase = 0U;
        nx_request_settle(&request->base, terminal, transferred);
    }
}

/** \brief A settled transport wins; cancellation never withdraws active DE. */
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = control(state);
    if (result != NX_SUCCESS || request == NULL) {
        return result == NX_SUCCESS ? NX_ERROR_INVALID : result;
    }
    if (nx_request_state(&request->base) == NX_REQUEST_SETTLED) {
        return NX_SUCCESS;
    }
    if (state->active != request) {
        return NX_ERROR_STATE;
    }
    if (state->phase == PHASE_SETUP) {
        if (nx_request_state(&state->inner.base) == NX_REQUEST_QUEUED) {
            nx_request_settle(&state->inner.base, NX_ERROR_CANCELLED, 0U);
        }
        begin_hold(state, NX_ERROR_CANCELLED, 0U, 0U);
        return NX_SUCCESS;
    }
    if (state->phase == PHASE_ACTIVE) {
        if (nx_request_state(&state->inner.base) == NX_REQUEST_SETTLED) {
            service(state);
            return NX_SUCCESS;
        }
        return nx_uart_port_cancel(state->config->transport, &state->inner);
    }
    return NX_SUCCESS;
}

/** \brief Close admission; hardware stop occurs only after DE and outer drain.
 */
static nx_result_t stop(void* context) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = control(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->closing = true;
    if (state->active != NULL) {
        result = cancel(state, state->active);
        if (result != NX_SUCCESS) {
            return result;
        }
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    result = nx_uart_port_stop(state->config->transport);
    if (result == NX_SUCCESS) {
        state->initialized = false;
    }
    return result;
}

/** \brief Read reception from the same exclusive half-duplex binding. */
static nx_result_t read_events(void* context, nx_uart_rx_event_t* events,
                               size_t capacity, size_t* count) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = control(state);
    return result == NX_SUCCESS
               ? nx_uart_port_read_events(state->config->transport, events,
                                          capacity, count)
               : result;
}

/** \brief Read independently selected receive bytes without a second ring. */
static nx_result_t read_bytes(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = control(state);
    return result == NX_SUCCESS
               ? nx_uart_port_read_bytes(state->config->transport, bytes,
                                         capacity, count)
               : result;
}

/** \brief The real UART/DMA sources still own the optional IRQ hint target. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = control(state);
    return result == NX_SUCCESS ? nx_uart_port_attach_wake(
                                      state->config->transport, wake, ceiling)
                                : result;
}

/** \brief Start independently selected RX without touching the DE output. */
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = control(state);
    if (result == NX_SUCCESS && state->closing) {
        result = NX_ERROR_STATE;
    }
    return result == NX_SUCCESS
               ? nx_uart_port_rx_start(state->config->transport, stream)
               : result;
}

/** \brief RX stop preserves the transport's producer/consumer loan contract. */
static nx_result_t rx_stop(void* context) {
    nx_uart_rs485_state_t* state = context;
    nx_result_t result = control(state);
    return result == NX_SUCCESS ? nx_uart_port_rx_stop(state->config->transport)
                                : result;
}

const nx_uart_ops_t nx_uart_rs485_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read_events = read_events,
    .read_bytes = read_bytes,
    .stop = stop,
    .attach_wake = attach_wake,
    .rx_start = rx_start,
    .rx_stop = rx_stop,
};
