/**
 * \file            uart_word.c
 * \brief           F103RB word transport with explicit wire and IRQ retirement.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_usart.h"
#include "system/platform.h"
#include "word.h"

#ifndef NX_STM32F1_UART_WORD_STATUS
#define NX_STM32F1_UART_WORD_STATUS()       (USART2->SR)
#define NX_STM32F1_UART_WORD_RECEIVE()      LL_USART_ReceiveData9(USART2)
#define NX_STM32F1_UART_WORD_TRANSMIT(word) LL_USART_TransmitData9(USART2, word)
#endif

#ifndef NX_STM32F1_WORD_IRQ_ENABLED
#define NX_STM32F1_WORD_IRQ_ENABLED()  NVIC_GetEnableIRQ(USART2_IRQn)
#define NX_STM32F1_WORD_IRQ_PENDING()  NVIC_GetPendingIRQ(USART2_IRQn)
#define NX_STM32F1_WORD_IRQ_ACTIVE()   NVIC_GetActive(USART2_IRQn)
#define NX_STM32F1_WORD_IRQ_PRIORITY() NVIC_GetPriority(USART2_IRQn)
#define NX_STM32F1_WORD_IRQ_SET_PRIORITY(value)                                \
    NVIC_SetPriority(USART2_IRQn, value)
#define NX_STM32F1_WORD_IRQ_ENABLE()  NVIC_EnableIRQ(USART2_IRQn)
#define NX_STM32F1_WORD_IRQ_DISABLE() NVIC_DisableIRQ(USART2_IRQn)
#define NX_STM32F1_WORD_IRQ_CLEAR()   NVIC_ClearPendingIRQ(USART2_IRQn)
#endif

#define USART_GATE RCC_APB1ENR_USART2EN
#define AFIO_GATE  RCC_APB2ENR_AFIOEN
#define USART_RX                                                               \
    ((uint32_t)(USART_SR_FE | USART_SR_NE | USART_SR_ORE | USART_SR_PE))
#define RX_STATUS (USART_RX | USART_SR_RXNE)
#define TX_IRQS   ((uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE))
#define USART_9N1                                                              \
    ((uint32_t)(USART_CR1_M | USART_CR1_UE | USART_CR1_TE | USART_CR1_RE |     \
                USART_CR1_RXNEIE))
#define TX_PAD                                                                 \
    (GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_LOW | LL_GPIO_OUTPUT_PUSHPULL)
#define RX_PAD LL_GPIO_MODE_FLOATING

static bool vector_quiet(void) {
    return !NX_STM32F1_WORD_IRQ_ENABLED() && !NX_STM32F1_WORD_IRQ_PENDING() &&
           !NX_STM32F1_WORD_IRQ_ACTIVE();
}

static bool apb1_hsi8(void) {
    return (RCC->CR & RCC_CR_HSIRDY) != 0U &&
           (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) ==
               RCC_CFGR_SWS_HSI;
}

static bool independent(const nx_stm32f1_uart_word_t* state, const void* data,
                        size_t bytes) {
    const uintptr_t begin = (uintptr_t)data;
    const uintptr_t end = begin + bytes;
    const nx_stm32f1_uart_word_config_t* config = state->config;
    return !(begin < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(begin < UINT32_C(0xf0000000) && end > UINT32_C(0xe0000000)) &&
           !nx_io_dispatch_overlap(data, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(data, bytes, config, sizeof(*config)) &&
           (config == NULL ||
            !nx_io_dispatch_overlap(data, bytes, config->storage,
                                    config->capacity * sizeof(uint16_t)));
}

/** \brief Reject a foreign owner and malformed generated storage without
 * writes. */
nx_result_t
nx_stm32f1_uart_word_admit(const nx_stm32f1_uart_word_t* state,
                           const nx_stm32f1_uart_word_config_t* config) {
    if (state == NULL || config == NULL || config->capacity == 0U ||
        config->capacity > 4096U || config->clock_hz != 8000000U ||
        config->baud < 1200U || config->baud > 500000U ||
        config->priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    const size_t bytes = config->capacity * sizeof(uint16_t);
    if (!nx_io_dispatch_span(config->storage, bytes, _Alignof(uint16_t)) ||
        !independent(state, config->storage, bytes) ||
        nx_io_dispatch_overlap(config->storage, bytes, config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    return state->config == NULL && vector_quiet() &&
                   (RCC->APB1RSTR & RCC_APB1RSTR_USART2RST) == 0U
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

/** \brief Verify the readable USART2 remap bit without writing SWJ
 * configuration. */
static nx_result_t default_route(nx_stm32f1_uart_word_t* state) {
    state->afio_gate_added = (RCC->APB2ENR & AFIO_GATE) == 0U;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, AFIO_GATE, AFIO_GATE))
        return NX_ERROR_IO;
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_USART2_REMAP) != 0U;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, AFIO_GATE, 0U))
        return NX_ERROR_IO;
    state->afio_gate_added = false;
    return remapped ? NX_ERROR_BUSY : NX_SUCCESS;
}

/** \brief Configure one owner while remembering only resources it changed. */
nx_result_t
nx_stm32f1_uart_word_prepare(nx_stm32f1_uart_word_t* state,
                             const nx_stm32f1_uart_word_config_t* config) {
    nx_result_t result = nx_stm32f1_uart_word_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_system_live() || !apb1_hsi8())
        return NX_ERROR_STATE;
    state->config = config;
    result = default_route(state);
    if (result != NX_SUCCESS) {
        if (!state->afio_gate_added)
            state->config = NULL;
        return result;
    }
    state->usart_gate_added = (RCC->APB1ENR & USART_GATE) == 0U;
    if (state->usart_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, USART_GATE, USART_GATE))
        return NX_ERROR_IO;
    if (USART2->CR1 != 0U || USART2->CR2 != 0U || USART2->CR3 != 0U ||
        USART2->BRR != 0U || USART2->GTPR != 0U ||
        (NX_STM32F1_UART_WORD_STATUS() & RX_STATUS) != 0U)
        return NX_ERROR_BUSY;

    result =
        nx_stm32f1_pads_capture(&state->pads, GPIOA, (1U << 2U) | (1U << 3U));
    if (result != NX_SUCCESS)
        return result;
    result =
        nx_stm32f1_pads_apply(&state->pads, (TX_PAD << 8U) | (RX_PAD << 12U),
                              0U, state->pads.latch | (1U << 2U));
    if (result != NX_SUCCESS)
        return result;

    const uint32_t brr = (config->clock_hz + config->baud / 2U) / config->baud;
    state->controls_owned = true;
    if (!nx_stm32f1_write_masked(&USART2->BRR, UINT16_MAX, brr) ||
        !nx_stm32f1_write_masked(&USART2->CR3, USART_CR3_EIE, USART_CR3_EIE) ||
        !nx_stm32f1_write_masked(&USART2->CR1, USART_9N1, USART_9N1))
        return NX_ERROR_IO;
    state->previous_priority = (uint8_t)NX_STM32F1_WORD_IRQ_PRIORITY();
    state->vector_owned = true;
    NX_STM32F1_WORD_IRQ_SET_PRIORITY(config->priority);
    if (NX_STM32F1_WORD_IRQ_PRIORITY() != config->priority)
        return NX_ERROR_IO;
    state->live = true;
    NX_STM32F1_WORD_IRQ_ENABLE();
    if (!NX_STM32F1_WORD_IRQ_ENABLED()) {
        state->live = false;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

static void drain(nx_stm32f1_uart_word_t* state, nx_result_t result,
                  nx_time_us_t now) {
    state->terminal = result;
    state->wire_done = state->sent == 0U;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    /* One word in TDR and one in the shifter, 11 bits each. */
    const uint32_t baud = state->config->baud;
    const uint32_t horizon = (22000000U + baud - 1U) / baud + 100U;
    if (nx_deadline_finite_after(now, horizon, &state->drain_deadline) !=
        NX_SUCCESS)
        state->drain_deadline = 0U;
    (void)nx_stm32f1_write_masked(&USART2->CR1, TX_IRQS,
                                  state->wire_done ? 0U : USART_CR1_TCIE);
}

static nx_result_t start(nx_stm32f1_uart_word_t* state,
                         nx_uart_word_tx_request_t* request,
                         nx_request_state_t origin) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!nx_io_dispatch_span(request, sizeof(*request),
                             _Alignof(nx_uart_word_tx_request_t)))
        return NX_ERROR_INVALID;
    if (!state->live || state->closing ||
        nx_request_state(&request->base) != origin)
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    if (request->words == 0U || request->words > SIZE_MAX / sizeof(uint16_t))
        return NX_ERROR_INVALID;
    const size_t bytes = request->words * sizeof(uint16_t);
    if (!nx_io_dispatch_span(request->data, bytes, _Alignof(uint16_t)) ||
        !independent(state, request, sizeof(*request)) ||
        !independent(state, request->data, bytes) ||
        nx_io_dispatch_overlap(request->data, bytes, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < request->words; ++i) {
        if (request->data[i] > UINT16_C(0x1ff))
            return NX_ERROR_INVALID;
    }

    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_time_us_t now = nx_time_now_us();
    nx_result_t result = NX_ERROR_TIMEOUT;
    if (!nx_deadline_expired(request->base.deadline, now)) {
        result = origin == NX_REQUEST_READY
                     ? nx_request_admit(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            state->active = request;
            state->sent = 0U;
            state->terminal = NX_SUCCESS;
            state->wire_done = false;
            if (!nx_stm32f1_write_masked(&USART2->CR1, TX_IRQS,
                                         USART_CR1_TXEIE))
                drain(state, NX_ERROR_IO, now);
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

static nx_result_t submit(void* context, nx_uart_word_tx_request_t* request) {
    return start(context, request, NX_REQUEST_READY);
}

static nx_result_t start_admitted(void* context,
                                  nx_uart_word_tx_request_t* request) {
    return start(context, request, NX_REQUEST_QUEUED);
}

static nx_result_t cancel(void* context, nx_uart_word_tx_request_t* request) {
    nx_stm32f1_uart_word_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!nx_io_dispatch_span(request, sizeof(*request),
                             _Alignof(nx_uart_word_tx_request_t)))
        return NX_ERROR_INVALID;
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (state->active == request) {
        if (!state->wire_done && state->terminal == NX_SUCCESS)
            drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
    } else if (nx_request_state(&request->base) != NX_REQUEST_SETTLED) {
        result = NX_ERROR_STATE;
    }
    nx_arch_irq_restore(saved);
    return result;
}

static void receive(nx_stm32f1_uart_word_t* state, uint32_t status) {
    const uint16_t word = NX_STM32F1_UART_WORD_RECEIVE();
    if (state->closing)
        return;
    const uint16_t capacity = state->config->capacity;
    if ((status & RX_STATUS) != USART_SR_RXNE || state->count == capacity ||
        state->loss) {
        state->loss = true;
        return;
    }
    const uint16_t end = (uint16_t)(state->head + state->count);
    state->config->storage[end < capacity ? end : end - capacity] = word;
    ++state->count;
}

static void completed(nx_stm32f1_uart_word_t* state, nx_time_us_t now) {
    if (state->terminal == NX_SUCCESS &&
        nx_deadline_expired(state->active->base.deadline, now))
        state->terminal = NX_ERROR_TIMEOUT;
    state->wire_done = true;
    if (!nx_stm32f1_write_masked(&USART2->CR1, TX_IRQS, 0U)) {
        /* A stuck TC source must not starve the task that retries withdrawal.
         */
        NX_STM32F1_WORD_IRQ_DISABLE();
        state->closing = true;
        if (state->terminal == NX_SUCCESS)
            state->terminal = NX_ERROR_IO;
        state->drain_deadline = now;
    }
}

/** \brief A DR write consumes its own SR observation; an old TC cannot retire
 * it. */
void nx_stm32f1_uart_word_irq(nx_stm32f1_uart_word_t* state) {
    if (!state->live)
        return;
    const uint32_t status = NX_STM32F1_UART_WORD_STATUS();
    const uint32_t interrupts = USART2->CR1;
    const bool received = (status & RX_STATUS) != 0U;
    if (received)
        receive(state, status);
    const nx_uart_word_tx_request_t* request = state->active;
    if (request == NULL || state->wire_done)
        return;
    if ((status & USART_SR_TXE) != 0U && (interrupts & USART_CR1_TXEIE) != 0U &&
        state->terminal == NX_SUCCESS && state->sent < request->words) {
        if (received)
            (void)NX_STM32F1_UART_WORD_STATUS();
        NX_STM32F1_UART_WORD_TRANSMIT(request->data[state->sent]);
        if (++state->sent == request->words &&
            !nx_stm32f1_write_masked(&USART2->CR1, TX_IRQS, USART_CR1_TCIE))
            drain(state, NX_ERROR_IO, nx_time_now_us());
        return;
    }
    if ((status & USART_SR_TC) != 0U && (interrupts & USART_CR1_TCIE) != 0U)
        completed(state, nx_time_now_us());
}

static void service(void* context) {
    nx_stm32f1_uart_word_t* state = context;
    if (!nx_stm32f1_cold_context())
        return;
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_uart_word_tx_request_t* request = state->active;
    if (request != NULL) {
        const nx_time_us_t now = nx_time_now_us();
        if (!state->wire_done && state->terminal == NX_SUCCESS &&
            nx_deadline_expired(request->base.deadline, now))
            drain(state, NX_ERROR_TIMEOUT, now);
        if (!state->wire_done &&
            (state->sent == request->words || state->terminal != NX_SUCCESS) &&
            (NX_STM32F1_UART_WORD_STATUS() & USART_SR_TC) != 0U)
            completed(state, now);
        if (state->wire_done &&
            nx_stm32f1_write_masked(&USART2->CR1, TX_IRQS, 0U) &&
            !NX_STM32F1_WORD_IRQ_ACTIVE()) {
            state->active = NULL;
            nx_request_settle(&request->base, state->terminal, state->sent);
        } else if (state->terminal != NX_SUCCESS &&
                   nx_deadline_expired(state->drain_deadline, now) &&
                   nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
    nx_arch_irq_restore(saved);
}

static bool output(const nx_stm32f1_uart_word_t* state, const void* data,
                   size_t bytes) {
    const nx_uart_word_tx_request_t* request = state->active;
    return independent(state, data, bytes) &&
           (request == NULL ||
            (!nx_io_dispatch_overlap(data, bytes, request, sizeof(*request)) &&
             !nx_io_dispatch_overlap(data, bytes, request->data,
                                     request->words * sizeof(uint16_t))));
}

static nx_result_t read_words(void* context, uint16_t* words, size_t capacity,
                              size_t* count) {
    nx_stm32f1_uart_word_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!state->live || state->closing)
        return NX_ERROR_STATE;
    if (capacity == 0U || capacity > SIZE_MAX / sizeof(*words))
        return NX_ERROR_INVALID;
    const size_t bytes = capacity * sizeof(*words);
    if (!nx_io_dispatch_span(words, bytes, _Alignof(uint16_t)) ||
        !nx_io_dispatch_span(count, sizeof(*count), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(words, bytes, count, sizeof(*count)) ||
        !output(state, words, bytes) || !output(state, count, sizeof(*count)))
        return NX_ERROR_INVALID;

    size_t copied = 0U;
    bool lost = false;
    while (copied < capacity) {
        const nx_arch_irq_state_t saved = nx_arch_irq_save();
        lost |= state->loss;
        if (state->count == 0U) {
            state->loss = false;
            nx_arch_irq_restore(saved);
            break;
        }
        const uint16_t word = state->config->storage[state->head];
        if (++state->head == state->config->capacity)
            state->head = 0U;
        if (--state->count == 0U)
            state->loss = false;
        nx_arch_irq_restore(saved);
        words[copied++] = word;
    }
    *count = copied;
    return lost           ? NX_ERROR_OVERFLOW
           : copied == 0U ? NX_ERROR_EMPTY
                          : NX_SUCCESS;
}

/** \brief Close admission first, then isolate the vector and restore controls.
 */
static nx_result_t stop(void* context) {
    nx_stm32f1_uart_word_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL)
        return NX_SUCCESS;
    state->closing = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL)
            return NX_ERROR_BUSY;
    }
    if (state->vector_owned) {
        NX_STM32F1_WORD_IRQ_DISABLE();
        nx_arch_dsb();
        if (NX_STM32F1_WORD_IRQ_ENABLED() || NX_STM32F1_WORD_IRQ_ACTIVE())
            return NX_ERROR_IO;
    }
    state->live = false;
    if (state->controls_owned) {
        if (!nx_stm32f1_write_masked(&USART2->CR1, USART_9N1 | TX_IRQS, 0U) ||
            !nx_stm32f1_write_masked(&USART2->CR3, USART_CR3_EIE, 0U) ||
            !nx_stm32f1_write_masked(&USART2->BRR, UINT16_MAX, 0U))
            return NX_ERROR_IO;
        if ((NX_STM32F1_UART_WORD_STATUS() & RX_STATUS) != 0U)
            (void)NX_STM32F1_UART_WORD_RECEIVE();
        if ((NX_STM32F1_UART_WORD_STATUS() & RX_STATUS) != 0U)
            return NX_ERROR_IO;
        state->controls_owned = false;
    }
    if (state->vector_owned) {
        NX_STM32F1_WORD_IRQ_CLEAR();
        NX_STM32F1_WORD_IRQ_SET_PRIORITY(state->previous_priority);
        nx_arch_dsb();
        if (!vector_quiet() ||
            NX_STM32F1_WORD_IRQ_PRIORITY() != state->previous_priority)
            return NX_ERROR_IO;
        state->vector_owned = false;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_uart_word_close(nx_stm32f1_uart_word_t* state) {
    const nx_result_t stopped = stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL)
        return stopped;
    const nx_result_t restored = nx_stm32f1_pads_restore(&state->pads);
    if (restored != NX_SUCCESS)
        return restored;
    if (state->usart_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, USART_GATE, 0U))
        return NX_ERROR_IO;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, AFIO_GATE, 0U))
        return NX_ERROR_IO;
    *state = (nx_stm32f1_uart_word_t){0};
    return NX_SUCCESS;
}

const nx_uart_word_ops_t nx_stm32f1_uart_word_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read = read_words,
    .stop = stop,
};
