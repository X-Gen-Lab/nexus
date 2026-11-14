/**
 * \file            uart.c
 * \brief           GD32 SPL UART with explicit IRQ facts and final settlement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32_reference.h"
#include "nexus/arch/arch.h"

#ifndef NX_GD32_IRQ_ENABLED
#define NX_GD32_IRQ_ENABLED()           (NVIC_GetEnableIRQ(USART0_IRQn) != 0U)
#define NX_GD32_IRQ_PENDING()           (NVIC_GetPendingIRQ(USART0_IRQn) != 0U)
#define NX_GD32_IRQ_ACTIVE()            (NVIC_GetActive(USART0_IRQn) != 0U)
#define NX_GD32_IRQ_PRIORITY()          NVIC_GetPriority(USART0_IRQn)
#define NX_GD32_IRQ_SET_PRIORITY(value) NVIC_SetPriority(USART0_IRQn, (value))
#define NX_GD32_IRQ_ENABLE()            NVIC_EnableIRQ(USART0_IRQn)
#define NX_GD32_IRQ_DISABLE()           NVIC_DisableIRQ(USART0_IRQn)
#define NX_GD32_IRQ_CLEAR()             NVIC_ClearPendingIRQ(USART0_IRQn)
#endif

/** \brief Task metadata changes share the controller's single executor. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Validate disjoint C storage without permitting integer wraparound. */
static bool overlaps(const void* left, size_t count, const void* right,
                     size_t size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return count > UINTPTR_MAX - a || size > UINTPTR_MAX - b ||
           (a < b + size && b < a + count);
}

/** \brief Static mode requires the explicit IRC16M run domain to remain live.
 */
static bool running_clock(void) {
    return (RCU_CTL & (RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB)) ==
               (RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB) &&
           (RCU_CFG0 & (RCU_CFG0_SCS | RCU_CFG0_SCSS | RCU_CFG0_AHBPSC |
                        RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC)) == 0U;
}

nx_result_t nx_gd32_uart_admit(nx_gd32_uart_state_t* state,
                               const nx_gd32_uart_config_t* config) {
    if (state == NULL || config == NULL || config->registers != USART0 ||
        config->baud < 1200U || config->baud > 1000000U ||
        config->rx_storage == NULL || config->rx_capacity == 0U ||
        config->rx_capacity > SIZE_MAX / sizeof(*config->rx_storage) ||
        config->priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->config != NULL || NX_GD32_IRQ_ENABLED() ||
        NX_GD32_IRQ_PENDING() || NX_GD32_IRQ_ACTIVE())
        return NX_ERROR_BUSY;
    if (overlaps(config->rx_storage,
                 config->rx_capacity * sizeof(*config->rx_storage), state,
                 sizeof(*state)) ||
        overlaps(config->rx_storage,
                 config->rx_capacity * sizeof(*config->rx_storage), config,
                 sizeof(*config)))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}

nx_result_t nx_gd32_uart_prepare(nx_gd32_uart_state_t* state,
                                 const nx_gd32_uart_config_t* config) {
    nx_result_t result = nx_gd32_uart_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    if (!running_clock() || (RCU_APB2EN & RCU_APB2EN_USART0EN) == 0U)
        return NX_ERROR_STATE;
    if (USART_CTL0(USART0) != 0U || USART_CTL1(USART0) != 0U ||
        USART_CTL2(USART0) != 0U || USART_BAUD(USART0) != 0U)
        return NX_ERROR_BUSY;
    state->config = config;
    state->active = NULL;
    state->transferred = state->head = state->tail = state->queued = 0U;
    state->terminal = state->draining = state->lost = false;
    state->closing = false;
    state->saved_irq_priority = (uint8_t)NX_GD32_IRQ_PRIORITY();
    usart_baudrate_set(USART0, config->baud);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_interrupt_enable(USART0, USART_INT_RBNE);
    usart_enable(USART0);
    NX_GD32_IRQ_SET_PRIORITY(config->priority);
    NX_GD32_IRQ_CLEAR();
    NX_GD32_IRQ_ENABLE();
    nx_arch_dsb();
    nx_arch_isb();
    return NX_GD32_IRQ_ENABLED() ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_gd32_uart_validate(void* context,
                                  const nx_uart_tx_request_t* request) {
    nx_gd32_uart_state_t* state = context;
    if (state == NULL || request == NULL)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL || state->closing || !running_clock())
        return NX_ERROR_STATE;
    const nx_gd32_uart_config_t* config = state->config;
    /* Check the descriptor before reading its fields. RX may publish into its
     * own ring even while TX is active, so descriptor storage is also a loan.
     */
    if (overlaps(request, sizeof(*request), state, sizeof(*state)) ||
        overlaps(request, sizeof(*request), config, sizeof(*config)) ||
        overlaps(request, sizeof(*request), config->rx_storage,
                 config->rx_capacity * sizeof(*config->rx_storage)))
        return NX_ERROR_INVALID;
    if (request->data == NULL || request->length == 0U)
        return NX_ERROR_INVALID;
    if (overlaps(request->data, request->length, request, sizeof(*request)) ||
        overlaps(request->data, request->length, state, sizeof(*state)) ||
        overlaps(request->data, request->length, config, sizeof(*config)) ||
        overlaps(request->data, request->length, config->rx_storage,
                 config->rx_capacity * sizeof(*config->rx_storage)))
        return NX_ERROR_INVALID;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    nx_request_state_t phase = nx_request_state(&request->base);
    if (phase != NX_REQUEST_READY && phase != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    return NX_SUCCESS;
}

/** \brief Connect one admitted request before enabling the TX publisher. */
static void begin(nx_gd32_uart_state_t* state, nx_uart_tx_request_t* request) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->active = request;
    state->transferred = 0U;
    state->terminal_result = NX_SUCCESS;
    state->terminal = state->draining = false;
    usart_interrupt_enable(USART0, USART_INT_TBE);
    nx_arch_irq_restore(saved);
}

/** \brief Direct admission borrows only after all rejectable checks. */
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    nx_result_t result = nx_gd32_uart_validate(context, request);
    if (result != NX_SUCCESS)
        return result;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS)
        begin(context, request);
    return result;
}

/** \brief Preserve the adapter's existing borrow without readmission. */
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    nx_result_t result = nx_gd32_uart_validate(context, request);
    if (result != NX_SUCCESS)
        return result;
    if (nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    result = nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS)
        begin(context, request);
    return result;
}

/** \brief Stop future payload reads; the wire's existing byte remains owned. */
static void drain(nx_gd32_uart_state_t* state, nx_result_t result) {
    if (state->terminal || state->draining)
        return;
    state->terminal_result = result;
    state->draining = true;
    usart_interrupt_disable(USART0, USART_INT_TBE);
    usart_interrupt_enable(USART0, USART_INT_TC);
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
}

/** \brief Cancel requests through the same serialized task executor. */
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_gd32_uart_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state == NULL || request == NULL || state->active != request)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    drain(state, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Publish only after IRQ metadata and hardware publishers detach. */
static void service(void* context) {
    nx_gd32_uart_state_t* state = context;
    if (state == NULL || state->config == NULL || !task())
        return;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_uart_tx_request_t* request = state->active;
    if (request != NULL) {
        if (!state->terminal && !state->draining &&
            nx_deadline_expired(request->base.deadline, nx_time_now_us()))
            drain(state, NX_ERROR_TIMEOUT);
        if (state->draining && usart_flag_get(USART0, USART_FLAG_TC) != RESET) {
            usart_interrupt_disable(USART0, USART_INT_TC);
            state->terminal = true;
        }
        if (state->terminal) {
            nx_result_t result = state->terminal_result;
            size_t transferred = state->transferred;
            state->active = NULL;
            state->terminal = state->draining = false;
            nx_request_settle(&request->base, result, transferred);
        }
    }
    nx_arch_irq_restore(saved);
}

void nx_gd32_uart_irq(nx_gd32_uart_state_t* state) {
    if (state == NULL || state->config == NULL)
        return;
    /* A single STAT0 snapshot followed by DATA preserves error/byte pairing.
     * SPL flag_get returns only bool, so it cannot provide that atomic view. */
    uint32_t status = USART_STAT0(USART0);
    uint32_t errors = status & (USART_STAT0_PERR | USART_STAT0_FERR |
                                USART_STAT0_NERR | USART_STAT0_ORERR);
    if ((status & USART_STAT0_RBNE) != 0U || errors != 0U) {
        uint8_t byte = (uint8_t)usart_data_receive(USART0);
        uint32_t flags =
            ((errors & USART_STAT0_PERR) ? NX_UART_EVENT_PARITY : 0U) |
            ((errors & USART_STAT0_FERR) ? NX_UART_EVENT_FRAMING : 0U) |
            ((errors & USART_STAT0_NERR) ? NX_UART_EVENT_NOISE : 0U) |
            ((errors & USART_STAT0_ORERR) ? NX_UART_EVENT_OVERRUN : 0U);
        if ((status & USART_STAT0_RBNE) == 0U)
            flags |= NX_UART_EVENT_NO_BYTE;
        if (state->lost || state->queued == state->config->rx_capacity) {
            state->lost = true;
        } else {
            state->config->rx_storage[state->head] =
                (nx_uart_rx_event_t){nx_time_now_us(), flags, byte};
            state->head = (state->head + 1U) % state->config->rx_capacity;
            ++state->queued;
        }
    }
    if (state->active == NULL || state->terminal)
        return;
    if (!state->draining && (status & USART_STAT0_TBE) != 0U &&
        (USART_CTL0(USART0) & USART_CTL0_TBEIE) != 0U &&
        state->transferred < state->active->length) {
        usart_data_transmit(USART0, state->active->data[state->transferred++]);
        if (state->transferred == state->active->length) {
            usart_interrupt_disable(USART0, USART_INT_TBE);
            usart_interrupt_enable(USART0, USART_INT_TC);
        }
        return;
    }
    if ((status & USART_STAT0_TC) != 0U &&
        (USART_CTL0(USART0) & USART_CTL0_TCIE) != 0U) {
        usart_interrupt_disable(USART0, USART_INT_TC);
        state->terminal = true;
    }
}

/** \brief Validate one immediate RX copy without borrowing the destination. */
static nx_result_t receive_admit(nx_gd32_uart_state_t* state,
                                 const void* output, size_t bytes,
                                 const size_t* count) {
    if (state == NULL || output == NULL || count == NULL || bytes == 0U)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL || state->closing)
        return NX_ERROR_STATE;
    const nx_gd32_uart_config_t* config = state->config;
    if (overlaps(output, bytes, state, sizeof(*state)) ||
        overlaps(output, bytes, config, sizeof(*config)) ||
        overlaps(output, bytes, config->rx_storage,
                 config->rx_capacity * sizeof(*config->rx_storage)) ||
        overlaps(output, bytes, count, sizeof(*count)) ||
        overlaps(count, sizeof(*count), state, sizeof(*state)) ||
        overlaps(count, sizeof(*count), config, sizeof(*config)) ||
        overlaps(count, sizeof(*count), config->rx_storage,
                 config->rx_capacity * sizeof(*config->rx_storage)))
        return NX_ERROR_INVALID;
    const nx_uart_tx_request_t* request = state->active;
    if (request != NULL &&
        (overlaps(output, bytes, request, sizeof(*request)) ||
         overlaps(output, bytes, request->data, request->length) ||
         overlaps(count, sizeof(*count), request, sizeof(*request)) ||
         overlaps(count, sizeof(*count), request->data, request->length)))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}

/** \brief Older events precede a separately retained loss boundary marker. */
static nx_result_t read_events(void* context, nx_uart_rx_event_t* output,
                               size_t capacity, size_t* count) {
    if (capacity > SIZE_MAX / sizeof(*output))
        return NX_ERROR_INVALID;
    nx_gd32_uart_state_t* state = context;
    nx_result_t result =
        receive_admit(state, output, capacity * sizeof(*output), count);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    size_t copied = 0U;
    bool lost = state->lost;
    while (copied < capacity && state->queued != 0U) {
        output[copied++] = state->config->rx_storage[state->tail];
        state->tail = (state->tail + 1U) % state->config->rx_capacity;
        --state->queued;
    }
    if (copied < capacity && state->lost) {
        output[copied++] = (nx_uart_rx_event_t){
            nx_time_now_us(), NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE, 0U};
        state->lost = false;
    }
    *count = copied;
    nx_arch_irq_restore(saved);
    return lost           ? NX_ERROR_OVERFLOW
           : copied != 0U ? NX_SUCCESS
                          : NX_ERROR_EMPTY;
}

/** \brief Byte consumers omit status-only events and receive aggregate loss. */
static nx_result_t read_bytes(void* context, uint8_t* output, size_t capacity,
                              size_t* count) {
    nx_gd32_uart_state_t* state = context;
    nx_result_t result = receive_admit(state, output, capacity, count);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    size_t copied = 0U;
    bool lost = state->lost;
    while (copied < capacity && state->queued != 0U) {
        nx_uart_rx_event_t event = state->config->rx_storage[state->tail];
        state->tail = (state->tail + 1U) % state->config->rx_capacity;
        --state->queued;
        lost |=
            (event.flags & (NX_UART_EVENT_OVERRUN | NX_UART_EVENT_LOSS)) != 0U;
        if ((event.flags & NX_UART_EVENT_NO_BYTE) == 0U)
            output[copied++] = event.byte;
    }
    state->lost = false;
    *count = copied;
    nx_arch_irq_restore(saved);
    return lost           ? NX_ERROR_OVERFLOW
           : copied != 0U ? NX_SUCCESS
                          : NX_ERROR_EMPTY;
}

nx_result_t nx_gd32_uart_close(nx_gd32_uart_state_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL)
        return NX_SUCCESS;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->closing = true;
    if (state->active != NULL) {
        drain(state, NX_ERROR_CANCELLED);
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    usart_interrupt_disable(USART0, USART_INT_RBNE);
    usart_interrupt_disable(USART0, USART_INT_TBE);
    usart_interrupt_disable(USART0, USART_INT_TC);
    usart_disable(USART0);
    nx_arch_dsb();
    if ((USART_CTL0(USART0) & (USART_CTL0_UEN | USART_CTL0_RBNEIE |
                               USART_CTL0_TBEIE | USART_CTL0_TCIE)) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    NX_GD32_IRQ_DISABLE();
    NX_GD32_IRQ_CLEAR();
    nx_arch_dsb();
    nx_arch_isb();
    if (NX_GD32_IRQ_ENABLED() || NX_GD32_IRQ_PENDING() ||
        NX_GD32_IRQ_ACTIVE()) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    NX_GD32_IRQ_SET_PRIORITY(state->saved_irq_priority);
    usart_deinit(USART0);
    state->config = NULL;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Public stop retains exactly the same controller owner. */
static nx_result_t stop(void* context) {
    return nx_gd32_uart_close(context);
}

const nx_uart_ops_t nx_gd32_uart_ops = {.submit = submit,
                                        .start_admitted = start_admitted,
                                        .cancel = cancel,
                                        .service = service,
                                        .read_events = read_events,
                                        .read_bytes = read_bytes,
                                        .stop = stop};
