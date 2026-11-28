/**
 * \file            uart_dma.c
 * \brief           F103 DMA1 Channel 7 with finite wire and memory drain.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "uart_dma.h"
#include "dispatch_checks.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_usart.h"
#include "system/platform.h"

#ifndef NX_STM32F1_DMA_CLEAR_TC
#define NX_STM32F1_DMA_CLEAR_TC(reg) LL_USART_ClearFlag_TC(reg)
#endif

#ifndef NX_STM32F1_IRQ_ENABLED
#define NX_STM32F1_IRQ_ENABLED(irq)             NVIC_GetEnableIRQ(irq)
#define NX_STM32F1_IRQ_PENDING(irq)             NVIC_GetPendingIRQ(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)              NVIC_GetActive(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)            NVIC_GetPriority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value) NVIC_SetPriority((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)              NVIC_EnableIRQ(irq)
#define NX_STM32F1_IRQ_DISABLE(irq)             NVIC_DisableIRQ(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)               NVIC_ClearPendingIRQ(irq)
#endif

#define TX_DMA_MODE                                                            \
    ((uint32_t)(DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE))
#define TX_DMA_ENABLE                                                          \
    ((uint32_t)(DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE))
#define TX_DMA_FLAGS ((uint32_t)(DMA_ISR_TCIF7 | DMA_ISR_HTIF7 | DMA_ISR_TEIF7))
#define TX_DMA_CLEAR                                                           \
    ((uint32_t)(DMA_IFCR_CTCIF7 | DMA_IFCR_CHTIF7 | DMA_IFCR_CTEIF7))

/** \brief Validate the exact fixed request mapping, with no register writes. */
nx_result_t
nx_stm32f1_uart_dma_admit(const nx_stm32f1_uart_dma_t* state,
                          const nx_stm32f1_uart_dma_config_t* config) {
    if (state == NULL || config == NULL || config->dma != DMA1 ||
        config->channel != DMA1_Channel7 || config->irq != DMA1_Channel7_IRQn ||
        config->regions == NULL || config->region_count == 0U ||
        config->region_count > 16U) {
        return NX_ERROR_INVALID;
    }
    if (state->config != NULL) {
        return NX_ERROR_STATE;
    }
    nx_result_t accepted = nx_stm32f1_uart_admit(&state->uart, &config->uart);
    if (accepted != NX_SUCCESS) {
        return accepted;
    }
    return NX_STM32F1_IRQ_ENABLED(config->irq) ||
                   NX_STM32F1_IRQ_PENDING(config->irq) ||
                   NX_STM32F1_IRQ_ACTIVE(config->irq)
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}

/** \brief Claim an idle lane and vector, then compose the base UART owner. */
nx_result_t
nx_stm32f1_uart_dma_prepare(nx_stm32f1_uart_dma_t* state,
                            const nx_stm32f1_uart_dma_config_t* config) {
    nx_result_t result = nx_stm32f1_uart_dma_admit(state, config);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    state->config = config;
    const uint32_t gate = RCC_AHBENR_DMA1EN;
    state->gate_added = (RCC->AHBENR & gate) == 0U;
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    const DMA_Channel_TypeDef* lane = config->channel;
    if (lane->CCR != 0U || lane->CNDTR != 0U || lane->CPAR != 0U ||
        lane->CMAR != 0U || (config->dma->ISR & TX_DMA_FLAGS) != 0U) {
        return NX_ERROR_BUSY;
    }
    state->lane_owned = true;
    result = nx_stm32f1_uart_prepare(&state->uart, &config->uart);
    if (result != NX_SUCCESS) {
        return result;
    }

    state->previous_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(config->irq);
    state->vector_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(config->irq, config->uart.priority);
    if (NX_STM32F1_IRQ_PRIORITY(config->irq) != config->uart.priority) {
        return NX_ERROR_IO;
    }
    state->live = true;
    NX_STM32F1_IRQ_ENABLE(config->irq);
    if (!NX_STM32F1_IRQ_ENABLED(config->irq)) {
        state->live = false;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Compare caller extents already checked by the base interface. */
static bool independent(const nx_stm32f1_uart_dma_t* state, const void* data,
                        size_t bytes) {
    return !nx_io_dispatch_overlap(data, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(data, bytes, state->config,
                                   sizeof(*state->config)) &&
           !nx_io_dispatch_overlap(data, bytes, state->config->regions,
                                   state->config->region_count *
                                       sizeof(*state->config->regions));
}

/** \brief Check memory reachability and the exact UART before DMA admission. */
nx_result_t nx_stm32f1_uart_dma_validate(void* context,
                                         const nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_dma_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!state->live || state->closing) {
        return NX_ERROR_STATE;
    }
    nx_result_t checked = nx_stm32f1_uart_validate(&state->uart, request);
    if (checked != NX_SUCCESS) {
        return checked;
    }
    if (request->length > UINT16_MAX ||
        !independent(state, request, sizeof(*request)) ||
        !independent(state, request->data, request->length)) {
        return NX_ERROR_INVALID;
    }
    if ((RCC->AHBENR & RCC_AHBENR_DMA1EN) == 0U) {
        return NX_ERROR_IO;
    }
    if (state->active != NULL ||
        (state->config->channel->CCR & TX_DMA_ENABLE) != 0U ||
        (state->config->uart.registers->CR3 & USART_CR3_DMAT) != 0U) {
        return NX_ERROR_BUSY;
    }
    return nx_dma_buffer_validate(state->config->regions,
                                  state->config->region_count, request->data,
                                  request->length, 1U, NX_DMA_TO_DEVICE);
}

/** \brief ES096 §2.4.1 requires specific events, never the CGIF group bit. */
static bool clear_lane(const nx_stm32f1_uart_dma_config_t* config) {
    NX_STM32F1_WRITE(&config->dma->IFCR, TX_DMA_CLEAR);
    NX_STM32F1_IRQ_CLEAR(config->irq);
    nx_arch_dsb();
    return (config->dma->ISR & TX_DMA_FLAGS) == 0U &&
           !NX_STM32F1_IRQ_PENDING(config->irq);
}

/** \brief Close hardware admission before observing transfer-error flags. */
static bool detach(const nx_stm32f1_uart_dma_config_t* config,
                   bool uart_owned) {
    USART_TypeDef* uart = config->uart.registers;
    DMA_Channel_TypeDef* lane = config->channel;
    const bool request_off =
        !uart_owned || (uart->CR3 & USART_CR3_DMAT) == 0U ||
        nx_stm32f1_write_masked(&uart->CR3, USART_CR3_DMAT, 0U);
    const bool engine_off =
        (lane->CCR & TX_DMA_ENABLE) == 0U ||
        nx_stm32f1_write_masked(&lane->CCR, TX_DMA_ENABLE, 0U);
    return request_off && engine_off;
}

/** \brief A stuck source must not starve the task that owns its retained loan.
 */
static void mask_fault(nx_stm32f1_uart_dma_t* state, IRQn_Type irq) {
    state->closing = true;
    NX_STM32F1_IRQ_DISABLE(irq);
    nx_arch_dsb();
}

/** \brief Stop this lane or keep the failing interrupt masked for task retry.
 */
static bool stop_lane(nx_stm32f1_uart_dma_t* state) {
    if (detach(state->config, true)) {
        return true;
    }
    mask_fault(state, state->config->irq);
    return false;
}

/** \brief Retain the loan until both bus and wire have safely stopped. */
static void begin_drain(nx_stm32f1_uart_dma_t* state, nx_result_t result,
                        nx_time_us_t now) {
    state->terminal = result;
    state->phase = NX_STM32F1_DMA_WITHDRAW;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    const uint32_t grace = 20000000U / state->config->uart.baud + 100U;
    if (nx_deadline_finite_after(now, grace, &state->drain_deadline) !=
        NX_SUCCESS) {
        state->drain_deadline = 0U;
    }
}

/** \brief Publish only hardware facts in IRQ; the task releases the borrow. */
static void advance(nx_stm32f1_uart_dma_t* state, nx_time_us_t now) {
    const nx_stm32f1_uart_dma_config_t* config = state->config;
    DMA_Channel_TypeDef* lane = config->channel;
    USART_TypeDef* uart = config->uart.registers;
    nx_uart_tx_request_t* active = state->active;
    if ((RCC->AHBENR & RCC_AHBENR_DMA1EN) == 0U) {
        if (state->phase == NX_STM32F1_DMA_MOVING) {
            begin_drain(state, NX_ERROR_IO, now);
        }
        state->closing = true;
        (void)nx_request_transition(&active->base, NX_REQUEST_QUARANTINED);
        return;
    }
    if (state->phase == NX_STM32F1_DMA_MOVING) {
        const uint32_t flags = config->dma->ISR;
        if ((flags & DMA_ISR_TEIF7) != 0U ||
            ((flags & DMA_ISR_TCIF7) != 0U && lane->CNDTR != 0U)) {
            begin_drain(state, NX_ERROR_IO, now);
        } else if ((flags & DMA_ISR_TCIF7) != 0U) {
            begin_drain(state, NX_SUCCESS, now);
        } else if (nx_deadline_expired(active->base.deadline, now)) {
            begin_drain(state, NX_ERROR_TIMEOUT, now);
        } else {
            return;
        }
    }
    if (state->phase == NX_STM32F1_DMA_WITHDRAW && stop_lane(state)) {
        /* The disabled lane cannot create another error after this snapshot. */
        if (((config->dma->ISR & DMA_ISR_TEIF7) != 0U ||
             lane->CNDTR > active->length) &&
            state->terminal == NX_SUCCESS) {
            state->terminal = NX_ERROR_IO;
        }
        if (clear_lane(config)) {
            state->phase = NX_STM32F1_DMA_WIRE;
        }
    }
    if (state->phase != NX_STM32F1_DMA_COMPLETE &&
        state->terminal == NX_SUCCESS &&
        nx_deadline_expired(active->base.deadline, now)) {
        state->terminal = NX_ERROR_TIMEOUT;
    }
    if (state->phase == NX_STM32F1_DMA_WIRE) {
        if (!state->requested || lane->CNDTR == active->length ||
            (uart->SR & USART_SR_TC) != 0U) {
            state->phase = NX_STM32F1_DMA_COMPLETE;
        } else if ((uart->CR1 & USART_CR1_TCIE) == 0U) {
            (void)nx_stm32f1_write_masked(&uart->CR1, USART_CR1_TCIE,
                                          USART_CR1_TCIE);
        }
    }
    if (state->phase == NX_STM32F1_DMA_COMPLETE) {
        if ((uart->CR1 & USART_CR1_TCIE) != 0U &&
            !nx_stm32f1_write_masked(&uart->CR1, USART_CR1_TCIE, 0U)) {
            mask_fault(state, config->uart.irq);
        }
    } else if (nx_deadline_expired(state->drain_deadline, now)) {
        if (state->terminal == NX_SUCCESS) {
            state->terminal = NX_ERROR_IO;
        }
        (void)nx_request_transition(&active->base, NX_REQUEST_QUARANTINED);
    }
}

/** \brief Configure a disabled lane and arm the request signal last. */
static bool arm(nx_stm32f1_uart_dma_t* state,
                const nx_uart_tx_request_t* active) {
    const nx_stm32f1_uart_dma_config_t* config = state->config;
    DMA_Channel_TypeDef* lane = config->channel;
    USART_TypeDef* uart = config->uart.registers;
    if (!clear_lane(config)) {
        return false;
    }
    const uint32_t address = (uint32_t)(uintptr_t)&uart->DR;
    const uint32_t data = (uint32_t)(uintptr_t)active->data;
    const uint32_t length = (uint32_t)active->length;
    NX_STM32F1_WRITE(&lane->CPAR, address);
    NX_STM32F1_WRITE(&lane->CMAR, data);
    NX_STM32F1_WRITE(&lane->CNDTR, length);
    NX_STM32F1_WRITE(&lane->CCR, TX_DMA_MODE);
    nx_arch_dsb();
    if (lane->CPAR != address || lane->CMAR != data || lane->CNDTR != length ||
        lane->CCR != TX_DMA_MODE) {
        return false;
    }
    NX_STM32F1_DMA_CLEAR_TC(uart);
    nx_arch_dmb();
    if ((uart->SR & USART_SR_TC) != 0U ||
        !nx_stm32f1_write_masked(&lane->CCR, DMA_CCR_EN, DMA_CCR_EN)) {
        return false;
    }
    state->requested = true;
    return nx_stm32f1_write_masked(&uart->CR3, USART_CR3_DMAT, USART_CR3_DMAT);
}

/** \brief Start only after the unique READY/QUEUED borrow is accepted. */
static nx_result_t start(nx_stm32f1_uart_dma_t* state,
                         nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t result = nx_stm32f1_uart_dma_validate(state, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        state->active = request;
        state->phase = NX_STM32F1_DMA_MOVING;
        state->requested = false;
        state->terminal = NX_SUCCESS;
        if (!arm(state, request)) {
            begin_drain(state, NX_ERROR_IO, nx_time_now_us());
        }
    }
    nx_arch_irq_restore(guard);
    return result;
}

/** \brief Admit a single direct caller request. */
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return start(context, request, false);
}

/** \brief Transfer an already established adapter borrow. */
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    return start(context, request, true);
}

/** \brief Latch cancellation while preserving an already observed result. */
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_dma_t* state = context;
    if (request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    const nx_request_state_t phase = nx_request_state(&request->base);
    nx_result_t result = NX_SUCCESS;
    if (state->active == request) {
        const nx_time_us_t now = nx_time_now_us();
        advance(state, now);
        if (state->phase != NX_STM32F1_DMA_COMPLETE &&
            state->terminal == NX_SUCCESS) {
            if (state->phase == NX_STM32F1_DMA_MOVING) {
                begin_drain(state, NX_ERROR_CANCELLED, now);
            } else {
                state->terminal = NX_ERROR_CANCELLED;
            }
            advance(state, now);
        }
    } else if (phase != NX_REQUEST_SETTLED) {
        result = NX_ERROR_STATE;
    }
    nx_arch_irq_restore(guard);
    return result;
}

/** \brief Verify that no pending or active DMA source can enter the old loan.
 */
static bool publication_ready(nx_stm32f1_uart_dma_t* state) {
    const nx_stm32f1_uart_dma_config_t* config = state->config;
    if ((RCC->AHBENR & RCC_AHBENR_DMA1EN) == 0U ||
        (config->uart.registers->CR1 & USART_CR1_TCIE) != 0U ||
        (config->uart.registers->CR3 & USART_CR3_DMAT) != 0U ||
        (config->channel->CCR & TX_DMA_ENABLE) != 0U ||
        NX_STM32F1_IRQ_ACTIVE(config->irq)) {
        return false;
    }
    const uint32_t flags = config->dma->ISR & TX_DMA_FLAGS;
    if ((flags & DMA_ISR_TEIF7) != 0U && state->terminal == NX_SUCCESS) {
        state->terminal = NX_ERROR_IO;
    }
    return (flags == 0U && !NX_STM32F1_IRQ_PENDING(config->irq)) ||
           clear_lane(config);
}

/** \brief Check original sources once, then release-publish the request. */
static void service(void* context) {
    nx_stm32f1_uart_dma_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return;
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_uart_tx_request_t* request = state->active;
    if (request != NULL) {
        advance(state, nx_time_now_us());
        if (state->phase == NX_STM32F1_DMA_COMPLETE) {
            if (publication_ready(state)) {
                const nx_result_t result = state->terminal;
                const size_t sent = result == NX_SUCCESS ? request->length : 0U;
                state->active = NULL;
                nx_request_settle(&request->base, result, sent);
            } else {
                (void)nx_request_transition(&request->base,
                                            NX_REQUEST_QUARANTINED);
            }
        }
    }
    nx_arch_irq_restore(guard);
}

/** \brief Observe only this lane's pending hardware flags. */
void nx_stm32f1_uart_dma_irq(nx_stm32f1_uart_dma_t* state) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (state->active != NULL) {
        advance(state, nx_time_now_us());
    }
    nx_arch_irq_restore(guard);
}

/** \brief Reuse the existing IRQ receive producer and qualify final TX TC. */
void nx_stm32f1_uart_dma_usart_irq(nx_stm32f1_uart_dma_t* state) {
    if (!state->uart.live) {
        return;
    }
    nx_stm32f1_uart_irq(&state->uart);
    if ((state->config->uart.registers->CR1 & USART_CR1_TCIE) != 0U) {
        nx_stm32f1_uart_dma_irq(state);
    }
}

/** \brief Reject writes into the DMA controller state or active TX borrow. */
static bool read_destination(const nx_stm32f1_uart_dma_t* state, void* output,
                             size_t bytes, size_t alignment, size_t* count) {
    if (!nx_io_dispatch_span(output, bytes, alignment) ||
        !nx_io_dispatch_span(count, sizeof(*count), _Alignof(size_t)) ||
        !independent(state, output, bytes) ||
        !independent(state, count, sizeof(*count))) {
        return false;
    }
    const nx_uart_tx_request_t* active = state->active;
    return active == NULL ||
           (!nx_io_dispatch_overlap(output, bytes, active, sizeof(*active)) &&
            !nx_io_dispatch_overlap(count, sizeof(*count), active,
                                    sizeof(*active)) &&
            !nx_io_dispatch_overlap(output, bytes, active->data,
                                    active->length) &&
            !nx_io_dispatch_overlap(count, sizeof(*count), active->data,
                                    active->length));
}

/** \brief Share the byte ring with the base provider under one consumer. */
static nx_result_t read_bytes(void* context, uint8_t* output, size_t capacity,
                              size_t* count) {
    nx_stm32f1_uart_dma_t* state = context;
    if (!state->live || state->closing) {
        return NX_ERROR_STATE;
    }
    if (!read_destination(state, output, capacity, 1U, count)) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32f1_uart_ops.read_bytes(&state->uart, output, capacity,
                                          count);
}

/** \brief Share the event ring while preserving the dedicated TX loan. */
static nx_result_t read_events(void* context, nx_uart_rx_event_t* output,
                               size_t capacity, size_t* count) {
    nx_stm32f1_uart_dma_t* state = context;
    if (!state->live || state->closing) {
        return NX_ERROR_STATE;
    }
    if (capacity > SIZE_MAX / sizeof(*output) ||
        !read_destination(state, output, capacity * sizeof(*output),
                          _Alignof(nx_uart_rx_event_t), count)) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32f1_uart_ops.read_events(&state->uart, output, capacity,
                                           count);
}

/** \brief Public stop drains and detaches, retaining cold shared resources. */
static nx_result_t stop(void* context) {
    nx_stm32f1_uart_dma_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config == NULL) {
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
    const nx_stm32f1_uart_dma_config_t* config = state->config;
    if (state->lane_owned &&
        ((RCC->AHBENR & RCC_AHBENR_DMA1EN) == 0U ||
         !detach(config, state->uart.controls_owned) || !clear_lane(config))) {
        return NX_ERROR_IO;
    }
    if (state->vector_owned) {
        NX_STM32F1_IRQ_DISABLE(config->irq);
        NX_STM32F1_IRQ_SET_PRIORITY(config->irq, state->previous_priority);
        nx_arch_dsb();
        if (NX_STM32F1_IRQ_ENABLED(config->irq) ||
            NX_STM32F1_IRQ_ACTIVE(config->irq) ||
            NX_STM32F1_IRQ_PRIORITY(config->irq) != state->previous_priority) {
            return NX_ERROR_IO;
        }
        state->vector_owned = false;
    }
    state->live = false;
    return nx_stm32f1_uart_ops.stop(&state->uart);
}

/** \brief Restore just this lane and release the acquired clock in reverse. */
nx_result_t nx_stm32f1_uart_dma_close(nx_stm32f1_uart_dma_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    const nx_result_t stopped = stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL) {
        return stopped;
    }
    if (state->lane_owned) {
        DMA_Channel_TypeDef* lane = state->config->channel;
        NX_STM32F1_WRITE(&lane->CCR, 0U);
        NX_STM32F1_WRITE(&lane->CNDTR, 0U);
        NX_STM32F1_WRITE(&lane->CPAR, 0U);
        NX_STM32F1_WRITE(&lane->CMAR, 0U);
        nx_arch_dsb();
        if (lane->CCR != 0U || lane->CNDTR != 0U || lane->CPAR != 0U ||
            lane->CMAR != 0U) {
            return NX_ERROR_IO;
        }
        state->lane_owned = false;
    }
    const nx_result_t closed = nx_stm32f1_uart_close(&state->uart);
    if (closed != NX_SUCCESS) {
        return closed;
    }
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN, 0U)) {
        return NX_ERROR_IO;
    }
    *state = (nx_stm32f1_uart_dma_t){0};
    return NX_SUCCESS;
}

const nx_uart_ops_t nx_stm32f1_uart_dma_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read_events = read_events,
    .read_bytes = read_bytes,
    .stop = stop,
};
