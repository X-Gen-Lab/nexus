/**
 * \file            uart_word.c
 * \brief           Independent Native nine-bit IRQ transport and drain model.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "uart_word_provider.h"

/** \brief Validate integer extents before any buffer arithmetic. */
static bool extent(const void* data, size_t count, size_t unit) {
    return data != NULL && count != 0U && count <= SIZE_MAX / unit &&
           count * unit <= UINTPTR_MAX - (uintptr_t)data;
}
/** \brief Both extents are validated before this cold overlap comparison. */
static bool overlap(const void* first, size_t first_size, const void* second,
                    size_t second_size) {
    return (uintptr_t)first < (uintptr_t)second + second_size &&
           (uintptr_t)second < (uintptr_t)first + first_size;
}
/** \brief No initialized instance is reset to revoke an existing IRQ owner. */
nx_result_t nx_native_uart_word_initialize(nx_native_uart_word_state_t* state,
                                           uint16_t* storage, size_t capacity,
                                           uint16_t* log, size_t log_capacity) {
    if (state == NULL || !extent(storage, capacity, sizeof(*storage)) ||
        (uintptr_t)storage % _Alignof(uint16_t) != 0U ||
        (log == NULL ? log_capacity != 0U
                     : (!extent(log, log_capacity, sizeof(*log)) ||
                        (uintptr_t)log % _Alignof(uint16_t) != 0U)) ||
        overlap(state, sizeof(*state), storage, capacity * sizeof(*storage)) ||
        (log != NULL &&
         (overlap(state, sizeof(*state), log, log_capacity * sizeof(*log)) ||
          overlap(storage, capacity * sizeof(*storage), log,
                  log_capacity * sizeof(*log))))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->initialized || state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    *state = (nx_native_uart_word_state_t){.storage = storage,
                                           .log = log,
                                           .capacity = capacity,
                                           .log_capacity = log_capacity,
                                           .initialized = true};
    return NX_SUCCESS;
}

/** \brief Admission has no effects until all descriptor/payload checks pass. */
static nx_result_t start(void* context, nx_uart_word_tx_request_t* request,
                         nx_request_state_t required) {
    nx_native_uart_word_state_t* state = context;
    if (state == NULL || request == NULL ||
        !extent(request->data, request->words, sizeof(*request->data)) ||
        (uintptr_t)request->data % _Alignof(uint16_t) != 0U ||
        overlap(request, sizeof(*request), request->data,
                request->words * sizeof(*request->data)) ||
        overlap(state, sizeof(*state), request->data,
                request->words * sizeof(*request->data)) ||
        overlap(state->storage, state->capacity * sizeof(uint16_t),
                request->data, request->words * sizeof(*request->data)) ||
        overlap(state, sizeof(*state), request, sizeof(*request)) ||
        overlap(state->storage, state->capacity * sizeof(uint16_t), request,
                sizeof(*request)) ||
        (state->log != NULL &&
         (overlap(state->log, state->log_capacity * sizeof(uint16_t),
                  request->data, request->words * sizeof(*request->data)) ||
          overlap(state->log, state->log_capacity * sizeof(uint16_t), request,
                  sizeof(*request))))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->closing ||
        nx_request_state(&request->base) != required) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    for (size_t i = 0U; i < request->words; ++i) {
        if (request->data[i] > 0x1FFU) {
            return NX_ERROR_INVALID;
        }
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result =
        required == NX_REQUEST_READY
            ? nx_request_admit(&request->base, NX_REQUEST_ACTIVE)
            : nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        state->active = request;
        state->position = 0U;
        state->terminal = NX_SUCCESS;
        state->tc = false;
    }
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief Unique direct admission. */
static nx_result_t submit(void* context, nx_uart_word_tx_request_t* request) {
    return start(context, request, NX_REQUEST_READY);
}
/** \brief Transfer an existing adapter borrow. */
static nx_result_t start_admitted(void* context,
                                  nx_uart_word_tx_request_t* request) {
    return start(context, request, NX_REQUEST_QUEUED);
}
/** \brief IRQ loads at most one word; TC is a later independent observation. */
void nx_native_uart_word_irq(nx_native_uart_word_state_t* state) {
    if (state == NULL || !state->initialized) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->active != NULL && state->terminal == NX_SUCCESS && !state->tc) {
        if (state->position < state->active->words) {
            uint16_t word = state->active->data[state->position++];
            if (state->log != NULL) {
                if (state->logged == state->log_capacity) {
                    state->terminal = NX_ERROR_OVERFLOW;
                } else {
                    state->log[state->logged++] = word;
                }
            }
        } else {
            state->tc = true;
        }
    }
    nx_arch_irq_restore(saved);
}
/** \brief Buffered overflow retains earlier words without silent overwrite. */
void nx_native_uart_word_receive(nx_native_uart_word_state_t* state,
                                 uint16_t word) {
    if (state == NULL || !state->initialized || state->closing) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (word > 0x1FFU || state->count == state->capacity || state->loss) {
        state->loss = true;
    } else {
        size_t tail = state->head + state->count;
        if (tail >= state->capacity) {
            tail -= state->capacity;
        }
        state->storage[tail] = word;
        ++state->count;
    }
    nx_arch_irq_restore(saved);
}
/** \brief Stop future loading; cancellation retains the active descriptor. */
static nx_result_t cancel(void* context, nx_uart_word_tx_request_t* request) {
    nx_native_uart_word_state_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (state->active != request) {
        result = nx_request_state(&request->base) == NX_REQUEST_SETTLED
                     ? NX_SUCCESS
                     : NX_ERROR_STATE;
    } else if (!state->tc && state->terminal == NX_SUCCESS) {
        state->terminal = NX_ERROR_CANCELLED;
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
    }
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief Detach references before release settlement, including failed drain.
 */
static void service(void* context) {
    nx_native_uart_word_state_t* state = context;
    if (state == NULL || nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_uart_word_tx_request_t* request = state->active;
    if (request != NULL) {
        if (!state->tc && state->terminal == NX_SUCCESS &&
            nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
            state->terminal = NX_ERROR_TIMEOUT;
        }
        if (state->tc || state->terminal != NX_SUCCESS) {
            if (state->hold_drain) {
                (void)nx_request_transition(&request->base,
                                            NX_REQUEST_QUARANTINED);
            } else {
                nx_result_t result = state->terminal;
                size_t transferred = state->position;
                state->active = NULL;
                nx_arch_dmb();
                nx_request_settle(&request->base, result, transferred);
            }
        }
    }
    nx_arch_irq_restore(saved);
}
/** \brief Copy bounded RX words under saved/restored metadata guards. */
static nx_result_t read_words(void* context, uint16_t* words, size_t capacity,
                              size_t* count) {
    nx_native_uart_word_state_t* state = context;
    if (state == NULL || !extent(count, 1U, sizeof(*count)) ||
        (uintptr_t)count % _Alignof(size_t) != 0U ||
        !extent(words, capacity, sizeof(*words)) ||
        (uintptr_t)words % _Alignof(uint16_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    size_t bytes = capacity * sizeof(*words);
    size_t rx_bytes = state->capacity * sizeof(uint16_t);
    if (overlap(state, sizeof(*state), words, bytes) ||
        overlap(state->storage, rx_bytes, words, bytes) ||
        overlap(count, sizeof(*count), words, bytes) ||
        overlap(state, sizeof(*state), count, sizeof(*count)) ||
        overlap(state->storage, rx_bytes, count, sizeof(*count)) ||
        (state->active != NULL &&
         (overlap(state->active, sizeof(*state->active), words, bytes) ||
          overlap(state->active->data, state->active->words * sizeof(uint16_t),
                  words, bytes) ||
          overlap(state->active, sizeof(*state->active), count,
                  sizeof(*count)) ||
          overlap(state->active->data, state->active->words * sizeof(uint16_t),
                  count, sizeof(*count)))) ||
        (state->log != NULL &&
         (overlap(state->log, state->log_capacity * sizeof(uint16_t), words,
                  bytes) ||
          overlap(state->log, state->log_capacity * sizeof(uint16_t), count,
                  sizeof(*count))))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized) {
        return NX_ERROR_STATE;
    }
    *count = 0U;
    bool loss = false;
    while (*count < capacity) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        loss |= state->loss;
        if (state->count == 0U) {
            state->loss = false;
            nx_arch_irq_restore(saved);
            break;
        }
        uint16_t word = state->storage[state->head];
        if (++state->head == state->capacity) {
            state->head = 0U;
        }
        --state->count;
        if (state->count == 0U) {
            state->loss = false;
        }
        nx_arch_irq_restore(saved);
        words[(*count)++] = word;
    }
    return loss ? NX_ERROR_OVERFLOW
                : (*count != 0U ? NX_SUCCESS : NX_ERROR_EMPTY);
}

/** \brief Explicit stop closes admission and preserves a failed drain borrow.
 */
static nx_result_t stop(void* context) {
    nx_native_uart_word_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    state->closing = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    state->initialized = false;
    return NX_SUCCESS;
}
const nx_uart_word_ops_t nx_native_uart_word_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read = read_words,
    .stop = stop,
};
