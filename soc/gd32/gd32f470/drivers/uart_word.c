/**
 * \file            uart_word.c
 * \brief           Independent nine-bit USART IRQ transport and true-TC drain.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../../../common/uart_control.h"
#include "gd32f470_uart_word.h"
#include "gd32f4xx.h"
#include "private/system.h"

/** \brief Validate extents without wrapping integer or pointer arithmetic. */
static bool extent(const void* data, size_t count, size_t unit) {
    return data != NULL && count != 0U && count <= SIZE_MAX / unit &&
           count * unit <= UINTPTR_MAX - (uintptr_t)data;
}
/** \brief Only validated extents reach overlap comparisons. */
static bool overlap(const void* first, size_t first_size, const void* second,
                    size_t second_size) {
    return (uintptr_t)first < (uintptr_t)second + second_size &&
           (uintptr_t)second < (uintptr_t)first + first_size;
}

/** \brief Reviewed GD register span ends at vendor USART_CHC offset C0. */
#define NX_GD32_UART_REGISTER_BYTES 0xC4U
/** \brief Immutable routing and mutable control storage are never payload. */
static bool control_overlap(const nx_gd32_uart_word_state_t* state,
                            const void* data, size_t bytes) {
    const nx_gd32_uart_controller_t* controller = state->controller;
    if (controller == NULL) {
        return false;
    }
    return overlap(controller, sizeof(*controller), data, bytes) ||
           overlap((const void*)(uintptr_t)controller->registers,
                   NX_GD32_UART_REGISTER_BYTES, data, bytes) ||
           overlap((const void*)(uintptr_t)&RCU_REG_VAL(controller->clock),
                   sizeof(uint32_t), data, bytes) ||
           overlap((const void*)(uintptr_t)&RCU_REG_VAL(controller->reset),
                   sizeof(uint32_t), data, bytes) ||
           overlap((const void*)(uintptr_t)NVIC, sizeof(*NVIC), data, bytes);
}
static nx_result_t stop(void* context);

/** \brief Cold readiness follows independent clock, USART and IRQ proofs. */
nx_result_t
nx_gd32_uart_word_initialize(nx_gd32_uart_word_state_t* state,
                             const nx_gd32_uart_controller_t* controller,
                             uint32_t baud, uint16_t* storage, size_t capacity,
                             unsigned priority) {
    if (state == NULL ||
        (controller != &nx_gd32_usart0_controller &&
         controller != &nx_gd32_usart1_controller) ||
        baud == 0U || baud > controller->clock_hz / 16U || priority > 15U ||
        !extent(storage, capacity, sizeof(*storage)) ||
        capacity > SIZE_MAX / 2U ||
        (uintptr_t)storage % _Alignof(uint16_t) != 0U ||
        overlap(state, sizeof(*state), storage, capacity * sizeof(*storage))) {
        return NX_ERROR_INVALID;
    }
    nx_gd32_uart_word_state_t selected = {.controller = controller};
    if (control_overlap(&selected, storage, capacity * sizeof(*storage)) ||
        control_overlap(&selected, state, sizeof(*state))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t divisor =
        controller->clock_hz / baud +
        (controller->clock_hz % baud >= baud / 2U + baud % 2U ? 1U : 0U);
    if (divisor == 0U || divisor > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (state->owned || state->initialized || state->active != NULL ||
        !NX_UART_IRQ_QUIET(controller->irq) ||
        (RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        return NX_ERROR_BUSY;
    }
    *state = (nx_gd32_uart_word_state_t){.controller = controller,
                                         .storage = storage,
                                         .capacity = capacity,
                                         .baud = baud,
                                         .initialized = true,
                                         .owned = true,
                                         .closing = true};
    NX_UART_CLOCK_ON(controller->clock);
    nx_gd32_peripheral_barrier();
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) == 0U) {
        goto failed;
    }
    /* An exclusive cold owner starts a new receive epoch. The vendor reset
     * discards unread data and RX errors retained across clock gating. */
    usart_deinit(controller->registers);
    nx_gd32_peripheral_barrier();
    if ((USART_STAT0(controller->registers) &
         (USART_STAT0_RBNE | USART_STAT0_PERR | USART_STAT0_FERR |
          USART_STAT0_ORERR | USART_STAT0_NERR)) != 0U) {
        goto failed;
    }
    NX_UART_REG_WRITE(&USART_CTL0(controller->registers), 0U);
    nx_gd32_peripheral_barrier();
    if (USART_CTL0(controller->registers) != 0U) {
        goto failed;
    }
    NX_UART_REG_WRITE(&USART_BAUD(controller->registers), divisor);
    NX_UART_REG_WRITE(&USART_CTL1(controller->registers), 0U);
    NX_UART_REG_WRITE(&USART_CTL2(controller->registers), USART_CTL2_ERRIE);
    uint32_t control = USART_CTL0_WL | USART_CTL0_TEN | USART_CTL0_REN |
                       USART_CTL0_RBNEIE | USART_CTL0_PERRIE;
    NX_UART_REG_WRITE(&USART_CTL0(controller->registers), control);
    nx_gd32_peripheral_barrier();
    if (USART_CTL0(controller->registers) != control ||
        USART_CTL1(controller->registers) != 0U ||
        USART_CTL2(controller->registers) != USART_CTL2_ERRIE ||
        USART_BAUD(controller->registers) != divisor) {
        goto failed;
    }
    NX_UART_REG_WRITE(&USART_CTL0(controller->registers),
                      control | USART_CTL0_UEN);
    nx_gd32_peripheral_barrier();
    if (USART_CTL0(controller->registers) != (control | USART_CTL0_UEN)) {
        goto failed;
    }
    NX_UART_IRQ_SET_PRIORITY(controller->irq, priority);
    NX_UART_IRQ_CLEAR_PENDING(controller->irq);
    NX_UART_IRQ_ENABLE(controller->irq);
    nx_gd32_peripheral_barrier();
    if (!NX_UART_IRQ_CONFIGURED(controller->irq, priority) ||
        NX_UART_IRQ_ACTIVE(controller->irq) != 0U) {
        goto failed;
    }
    state->closing = false;
    return NX_SUCCESS;
failed:
    (void)stop(state);
    return NX_ERROR_IO;
}

/** \brief Validate the entire request before establishing its unique borrow. */
static nx_result_t start(void* context, nx_uart_word_tx_request_t* request,
                         nx_request_state_t required) {
    nx_gd32_uart_word_state_t* state = context;
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
        control_overlap(state, request, sizeof(*request)) ||
        control_overlap(state, request->data,
                        request->words * sizeof(*request->data))) {
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
        state->drain_deadline = NX_DEADLINE_NEVER;
        USART_STAT0(state->controller->registers) &= ~(uint32_t)USART_STAT0_TC;
        NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                          USART_CTL0(state->controller->registers) |
                              (USART_CTL0_TBEIE));
    }
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief The direct path admits exactly once. */
static nx_result_t submit(void* context, nx_uart_word_tx_request_t* request) {
    return start(context, request, NX_REQUEST_READY);
}
/** \brief The adapter path transfers an existing loan without readmission. */
static nx_result_t start_admitted(void* context,
                                  nx_uart_word_tx_request_t* request) {
    return start(context, request, NX_REQUEST_QUEUED);
}

/** \brief Stop future payload reads while retaining the inflight-word loan. */
static void begin_drain(nx_gd32_uart_word_state_t* state,
                        nx_result_t terminal) {
    NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                      USART_CTL0(state->controller->registers) &
                          (~(uint32_t)USART_CTL0_TBEIE));
    state->terminal = terminal;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    state->drain_deadline = nx_deadline_after(
        nx_time_now_us(), 22000000U / state->baud +
                              (22000000U % state->baud != 0U ? 1U : 0U) + 100U);
    if (state->position == 0U ||
        (USART_STAT0(state->controller->registers) & USART_STAT0_TC) != 0U) {
        state->tc = true;
    } else {
        NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                          USART_CTL0(state->controller->registers) |
                              (USART_CTL0_TCIE));
    }
}
/** \brief Cancellation is an owner command, never a storage-release event. */
static nx_result_t cancel(void* context, nx_uart_word_tx_request_t* request) {
    nx_gd32_uart_word_state_t* state = context;
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
        begin_drain(state, NX_ERROR_CANCELLED);
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Queue a reliable RX word; errors/overflow never overwrite old data.
 */
static void receive(nx_gd32_uart_word_state_t* state, uint16_t word,
                    bool reliable) {
    if (!reliable || state->count == state->capacity || state->loss) {
        state->loss = true;
        return;
    }
    size_t tail = state->head + state->count;
    if (tail >= state->capacity) {
        tail -= state->capacity;
    }
    state->storage[tail] = word;
    ++state->count;
}

/** \brief Status/data clear ordering preserves ninth-bit receive data. */
void nx_gd32_uart_word_irq(nx_gd32_uart_word_state_t* state) {
    if (state == NULL || !state->initialized) {
        return;
    }
    uint32_t uart = state->controller->registers;
    uint32_t status = USART_STAT0(uart);
    if ((status & (USART_STAT0_RBNE | USART_STAT0_PERR | USART_STAT0_FERR |
                   USART_STAT0_ORERR | USART_STAT0_NERR)) != 0U) {
        uint16_t word = usart_data_receive(uart);
        bool reliable = (status & USART_STAT0_RBNE) != 0U &&
                        (status & (USART_STAT0_PERR | USART_STAT0_FERR |
                                   USART_STAT0_ORERR | USART_STAT0_NERR)) == 0U;
        receive(state, word, reliable);
    }
    nx_uart_word_tx_request_t* request = state->active;
    bool wrote = false;
    if (request != NULL && !state->tc && state->terminal == NX_SUCCESS &&
        state->position < request->words && (status & USART_STAT0_TBE) != 0U &&
        (USART_CTL0(uart) & USART_CTL0_TBEIE) != 0U) {
        usart_data_transmit(uart, request->data[state->position++]);
        wrote = true;
        if (state->position == request->words) {
            NX_UART_REG_WRITE(&USART_CTL0(uart),
                              USART_CTL0(uart) & (~(uint32_t)USART_CTL0_TBEIE));
            NX_UART_REG_WRITE(&USART_CTL0(uart),
                              USART_CTL0(uart) | (USART_CTL0_TCIE));
        }
    }
    if (request != NULL && !wrote && !state->tc &&
        (status & USART_STAT0_TC) != 0U &&
        (USART_CTL0(uart) & USART_CTL0_TCIE) != 0U &&
        (USART_CTL0(uart) & USART_CTL0_TBEIE) == 0U) {
        NX_UART_REG_WRITE(&USART_CTL0(uart),
                          USART_CTL0(uart) & (~(uint32_t)USART_CTL0_TCIE));
        if (state->terminal == NX_SUCCESS &&
            nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
            state->terminal = NX_ERROR_TIMEOUT;
        }
        state->tc = true;
    }
}

/** \brief TC, failed drain, and request settlement are distinct facts. */
static void service(void* context) {
    nx_gd32_uart_word_state_t* state = context;
    if (state == NULL || !state->initialized || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_uart_word_tx_request_t* request = state->active;
    if (request == NULL) {
        nx_arch_irq_restore(saved);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    if (!state->tc && state->terminal == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT);
    }
    if (!state->tc && state->terminal != NX_SUCCESS &&
        (USART_STAT0(state->controller->registers) & USART_STAT0_TC) != 0U) {
        state->tc = true;
    }
    if (!state->tc) {
        if (state->terminal != NX_SUCCESS &&
            nx_deadline_expired(state->drain_deadline, now)) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
        nx_arch_irq_restore(saved);
        return;
    }
    NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                      USART_CTL0(state->controller->registers) &
                          (~(uint32_t)(USART_CTL0_TBEIE | USART_CTL0_TCIE)));
    nx_gd32_peripheral_barrier();
    if ((USART_CTL0(state->controller->registers) &
         (USART_CTL0_TBEIE | USART_CTL0_TCIE)) != 0U) {
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        nx_arch_irq_restore(saved);
        return;
    }
    nx_result_t result = state->terminal;
    size_t transferred = state->position;
    state->active = NULL;
    nx_arch_dmb();
    nx_request_settle(&request->base, result, transferred);
    nx_arch_irq_restore(saved);
}
/** \brief Copy only caller destinations disjoint from live provider storage. */
static nx_result_t read_words(void* context, uint16_t* words, size_t capacity,
                              size_t* count) {
    nx_gd32_uart_word_state_t* state = context;
    if (state == NULL || !extent(count, 1U, sizeof(*count)) ||
        (uintptr_t)count % _Alignof(size_t) != 0U ||
        !extent(words, capacity, sizeof(*words)) ||
        (uintptr_t)words % _Alignof(uint16_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    size_t bytes = capacity * sizeof(*words);
    size_t rx_bytes = state->capacity * sizeof(uint16_t);
    if (control_overlap(state, words, bytes) ||
        control_overlap(state, count, sizeof(*count))) {
        return NX_ERROR_INVALID;
    }
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
                  count, sizeof(*count))))) {
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

/** \brief Close admission first; stop cannot revoke an unproved wire drain. */
static nx_result_t stop(void* context) {
    nx_gd32_uart_word_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->owned) {
        return NX_SUCCESS;
    }
    state->closing = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_gd32_uart_controller_t* controller = state->controller;
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        NX_UART_REG_WRITE(&USART_CTL0(controller->registers), 0U);
        NX_UART_REG_WRITE(&USART_CTL2(controller->registers), 0U);
        nx_gd32_peripheral_barrier();
        if (USART_CTL0(controller->registers) != 0U ||
            USART_CTL2(controller->registers) != 0U) {
            nx_arch_irq_restore(saved);
            return NX_ERROR_BUSY;
        }
    }
    NX_UART_IRQ_DISABLE(controller->irq);
    NX_UART_IRQ_CLEAR_PENDING(controller->irq);
    nx_gd32_peripheral_barrier();
    if (!NX_UART_IRQ_QUIET(controller->irq)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    NX_UART_CLOCK_OFF(controller->clock);
    nx_gd32_peripheral_barrier();
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    state->initialized = false;
    state->owned = false;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

const nx_uart_word_ops_t nx_gd32_uart_word_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read = read_words,
    .stop = stop,
};
