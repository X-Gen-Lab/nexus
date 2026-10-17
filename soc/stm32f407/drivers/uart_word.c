/**
 * \file            uart_word.c
 * \brief           Independent nine-bit USART IRQ transport and true-TC drain.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../../common/uart_control.h"
#include "stm32f407_uart_word.h"
#include "stm32f4xx_ll_usart.h"
#ifndef NX_STM32_UART_ENABLE
#define NX_STM32_UART_ENABLE(registers) LL_USART_Enable(registers)
#endif

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

/** \brief Known hardware-control storage cannot be a caller payload. */
static bool control_overlap(const nx_stm32_uart_word_state_t* state,
                            const void* data, size_t bytes) {
    if (state->registers != NULL &&
        overlap(state->registers, sizeof(*state->registers), data, bytes)) {
        return true;
    }
    return overlap((const void*)(uintptr_t)&RCC->APB1ENR, sizeof(RCC->APB1ENR),
                   data, bytes) ||
           overlap((const void*)(uintptr_t)&RCC->APB2ENR, sizeof(RCC->APB2ENR),
                   data, bytes) ||
           overlap((const void*)(uintptr_t)&RCC->APB1RSTR,
                   sizeof(RCC->APB1RSTR), data, bytes) ||
           overlap((const void*)(uintptr_t)&RCC->APB2RSTR,
                   sizeof(RCC->APB2RSTR), data, bytes) ||
           overlap((const void*)(uintptr_t)NVIC_BASE, sizeof(NVIC_Type), data,
                   bytes);
}
static nx_result_t stop(void* context);

/** \brief Acquire the exact cold lease and validate hardware before ready. */
nx_result_t nx_stm32_uart_word_initialize(nx_stm32_uart_word_state_t* state,
                                          uint32_t clock_hz, uint32_t baud,
                                          uint16_t* storage, size_t capacity) {
    if (state == NULL || state->registers == NULL || baud == 0U ||
        baud > clock_hz / 16U || state->irq < 0 || state->irq >= 82 ||
        !extent(storage, capacity, sizeof(*storage)) ||
        capacity > SIZE_MAX / 2U ||
        (uintptr_t)storage % _Alignof(uint16_t) != 0U ||
        overlap(state, sizeof(*state), storage, capacity * sizeof(*storage)) ||
        control_overlap(state, storage, capacity * sizeof(*storage)) ||
        control_overlap(state, state, sizeof(*state))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t divisor =
        clock_hz / baud + (clock_hz % baud >= baud / 2U + baud % 2U ? 1U : 0U);
    if (divisor == 0U || divisor > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (state->owned || state->initialized || state->active != NULL ||
        !NX_UART_IRQ_QUIET(state->irq) ||
        (state->registers->CR1 &
         (USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE |
          USART_CR1_IDLEIE)) != 0U ||
        (state->registers->CR3 & (USART_CR3_DMAR | USART_CR3_DMAT)) != 0U) {
        return NX_ERROR_BUSY;
    }
    USART_TypeDef* registers = state->registers;
    IRQn_Type irq = state->irq;
    *state = (nx_stm32_uart_word_state_t){.registers = registers,
                                          .irq = irq,
                                          .storage = storage,
                                          .capacity = capacity,
                                          .baud = baud,
                                          .initialized = true,
                                          .owned = true,
                                          .closing = true};
    NX_UART_REG_WRITE(&registers->CR1, 0U);
    nx_arch_dsb();
    if (registers->CR1 != 0U) {
        goto failed;
    }
    NX_UART_REG_WRITE(&registers->CR2, 0U);
    NX_UART_REG_WRITE(&registers->CR3, USART_CR3_EIE);
    NX_UART_REG_WRITE(&registers->BRR, divisor);
    (void)registers->SR;
    (void)registers->DR;
    uint32_t selected = USART_CR1_M | USART_CR1_TE | USART_CR1_RE |
                        USART_CR1_RXNEIE | USART_CR1_PEIE;
    NX_UART_REG_WRITE(&registers->CR1, selected);
    nx_arch_dsb();
    if (registers->CR1 != selected || registers->CR2 != 0U ||
        registers->CR3 != USART_CR3_EIE || registers->BRR != divisor) {
        goto failed;
    }
    NX_STM32_UART_ENABLE(registers);
    nx_arch_dsb();
    if (registers->CR1 != (selected | USART_CR1_UE)) {
        goto failed;
    }
    NX_UART_IRQ_SET_PRIORITY(irq, 5U);
    NX_UART_IRQ_CLEAR_PENDING(irq);
    NX_UART_IRQ_ENABLE(irq);
    nx_arch_dsb();
    if (!NX_UART_IRQ_CONFIGURED(irq, 5U) || NX_UART_IRQ_ACTIVE(irq) != 0U) {
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
    nx_stm32_uart_word_state_t* state = context;
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
        state->registers->SR &= ~(uint32_t)USART_SR_TC;
        NX_UART_REG_WRITE(&state->registers->CR1,
                          state->registers->CR1 | (USART_CR1_TXEIE));
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
static void begin_drain(nx_stm32_uart_word_state_t* state,
                        nx_result_t terminal) {
    NX_UART_REG_WRITE(&state->registers->CR1,
                      state->registers->CR1 & (~(uint32_t)USART_CR1_TXEIE));
    state->terminal = terminal;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    state->drain_deadline = nx_deadline_after(
        nx_time_now_us(), 22000000U / state->baud +
                              (22000000U % state->baud != 0U ? 1U : 0U) + 100U);
    if (state->position == 0U || (state->registers->SR & USART_SR_TC) != 0U) {
        state->tc = true;
    } else {
        NX_UART_REG_WRITE(&state->registers->CR1,
                          state->registers->CR1 | (USART_CR1_TCIE));
    }
}
/** \brief Cancellation is an owner command, never a storage-release event. */
static nx_result_t cancel(void* context, nx_uart_word_tx_request_t* request) {
    nx_stm32_uart_word_state_t* state = context;
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
static void receive(nx_stm32_uart_word_state_t* state, uint16_t word,
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

/** \brief Snapshot status once; never mistake old TC for a just-loaded word. */
void nx_stm32_uart_word_irq(nx_stm32_uart_word_state_t* state) {
    if (state == NULL || !state->initialized) {
        return;
    }
    uint32_t status = state->registers->SR;
    if ((status & (USART_SR_RXNE | USART_SR_PE | USART_SR_FE | USART_SR_ORE |
                   USART_SR_NE)) != 0U) {
        uint16_t word = LL_USART_ReceiveData9(state->registers);
        bool reliable = (status & USART_SR_RXNE) != 0U &&
                        (status & (USART_SR_PE | USART_SR_FE | USART_SR_ORE |
                                   USART_SR_NE)) == 0U;
        receive(state, word, reliable);
    }
    nx_uart_word_tx_request_t* request = state->active;
    bool wrote = false;
    if (request != NULL && !state->tc && state->terminal == NX_SUCCESS &&
        state->position < request->words && (status & USART_SR_TXE) != 0U &&
        (state->registers->CR1 & USART_CR1_TXEIE) != 0U) {
        LL_USART_TransmitData9(state->registers,
                               request->data[state->position++]);
        wrote = true;
        if (state->position == request->words) {
            NX_UART_REG_WRITE(&state->registers->CR1,
                              state->registers->CR1 &
                                  (~(uint32_t)USART_CR1_TXEIE));
            NX_UART_REG_WRITE(&state->registers->CR1,
                              state->registers->CR1 | (USART_CR1_TCIE));
        }
    }
    if (request != NULL && !wrote && !state->tc &&
        (status & USART_SR_TC) != 0U &&
        (state->registers->CR1 & USART_CR1_TCIE) != 0U &&
        (state->registers->CR1 & USART_CR1_TXEIE) == 0U) {
        NX_UART_REG_WRITE(&state->registers->CR1,
                          state->registers->CR1 & (~(uint32_t)USART_CR1_TCIE));
        if (state->terminal == NX_SUCCESS &&
            nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
            state->terminal = NX_ERROR_TIMEOUT;
        }
        state->tc = true;
    }
}

/** \brief TC, failed drain, and request settlement are distinct facts. */
static void service(void* context) {
    nx_stm32_uart_word_state_t* state = context;
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
        (state->registers->SR & USART_SR_TC) != 0U) {
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
    NX_UART_REG_WRITE(&state->registers->CR1,
                      state->registers->CR1 &
                          (~(uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE)));
    nx_arch_dsb();
    if ((state->registers->CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE)) != 0U) {
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
    nx_stm32_uart_word_state_t* state = context;
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
    nx_stm32_uart_word_state_t* state = context;
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
    NX_UART_REG_WRITE(&state->registers->CR1, 0U);
    NX_UART_REG_WRITE(&state->registers->CR3, 0U);
    nx_arch_dsb();
    if (state->registers->CR1 != 0U || state->registers->CR3 != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    NX_UART_IRQ_DISABLE(state->irq);
    NX_UART_IRQ_CLEAR_PENDING(state->irq);
    nx_arch_dsb();
    if (!NX_UART_IRQ_QUIET(state->irq)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    state->initialized = false;
    state->owned = false;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

const nx_uart_word_ops_t nx_stm32_uart_word_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read = read_words,
    .stop = stop,
};
