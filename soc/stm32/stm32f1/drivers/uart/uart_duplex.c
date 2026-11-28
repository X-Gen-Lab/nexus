/**
 * \file            uart_duplex.c
 * \brief           F103 USART2 shares one NVIC and pad owner across two lanes.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "uart_duplex.h"
#include "dispatch_checks.h"
#include "private/hardware.h"

#if !defined(STM32F103xB)
#error                                                                         \
    "Finite USART2 duplex DMA is reviewed for the STM32F103xB startup profile"
#endif

nx_result_t
nx_stm32f1_uart_duplex_admit(const nx_stm32f1_uart_duplex_t* state,
                             const nx_stm32f1_uart_duplex_config_t* config) {
    if (state == NULL || config == NULL ||
        config->rx_channel != DMA1_Channel6 ||
        config->rx_irq != DMA1_Channel6_IRQn ||
        config->tx.uart.registers != USART2 ||
        config->tx.uart.tx_registers != GPIOA || config->tx.uart.tx_pin != 2U ||
        config->tx.uart.profile != NX_UART_RX_BLOCKS ||
        config->tx.uart.clock_hz != 8000000U ||
        config->tx.uart.wire != NX_STM32F1_UART_WIRE_NONE ||
        config->tx.uart.frame != 0U)
        return NX_ERROR_INVALID;
    const nx_result_t tx = nx_stm32f1_uart_dma_admit(&state->tx, &config->tx);
    return tx == NX_SUCCESS ? nx_stm32f1_uart_rx_engine_admit() : tx;
}

nx_result_t
nx_stm32f1_uart_duplex_prepare(nx_stm32f1_uart_duplex_t* state,
                               const nx_stm32f1_uart_duplex_config_t* config) {
    nx_result_t result = nx_stm32f1_uart_duplex_admit(state, config);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_uart_dma_prepare(&state->tx, &config->tx);
    return result == NX_SUCCESS ? nx_stm32f1_uart_rx_engine_prepare(
                                      &state->rx, config->tx.uart.priority)
                                : result;
}

nx_result_t
nx_stm32f1_uart_duplex_validate(void* context,
                                const nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_duplex_t* state = context;
    if (state == NULL || request == NULL)
        return NX_ERROR_INVALID;
    if (state->tx.closing || state->rx.closing || !state->rx.live)
        return NX_ERROR_STATE;
    const nx_result_t result =
        nx_stm32f1_uart_dma_validate(&state->tx, request);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32f1_uart_dma_config_t* config = state->tx.config;
    /* The generated TX config is the first member of the whole receipt. */
    return nx_io_dispatch_overlap(request, sizeof(*request), state,
                                  sizeof(*state)) ||
                   nx_io_dispatch_overlap(request->data, request->length, state,
                                          sizeof(*state)) ||
                   nx_io_dispatch_overlap(
                       request, sizeof(*request), config,
                       sizeof(nx_stm32f1_uart_duplex_config_t)) ||
                   nx_io_dispatch_overlap(
                       request->data, request->length, config,
                       sizeof(nx_stm32f1_uart_duplex_config_t)) ||
                   nx_stm32f1_uart_stream_contains(state->rx.stream, request,
                                                   sizeof(*request)) ||
                   nx_stm32f1_uart_stream_contains(
                       state->rx.stream, request->data, request->length)
               ? NX_ERROR_INVALID
               : NX_SUCCESS;
}

static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_duplex_t* state = context;
    const nx_result_t result = nx_stm32f1_uart_duplex_validate(state, request);
    return result == NX_SUCCESS
               ? nx_stm32f1_uart_tx_engine_commit(&state->tx, request, false,
                                                  &state->rx.closing)
               : result;
}

static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_duplex_t* state = context;
    const nx_result_t result = nx_stm32f1_uart_duplex_validate(state, request);
    return result == NX_SUCCESS
               ? nx_stm32f1_uart_tx_engine_commit(&state->tx, request, true,
                                                  &state->rx.closing)
               : result;
}

static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_duplex_t* state = context;
    return nx_stm32f1_uart_tx_engine_cancel(&state->tx, request);
}

static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_stm32f1_uart_duplex_t* state = context;
    const nx_stm32f1_uart_dma_config_t* config = state->tx.config;
    if (state->tx.closing || config == NULL)
        return NX_ERROR_STATE;
    const nx_stm32f1_uart_rx_storage_t storage = {
        config->regions,
        config->region_count,
        state,
        sizeof(*state),
        config,
        sizeof(nx_stm32f1_uart_duplex_config_t),
        state->tx.active,
        &state->tx.closing,
    };
    return nx_stm32f1_uart_rx_engine_start(&state->rx, stream, &storage, true);
}

static nx_result_t rx_stop(void* context) {
    nx_stm32f1_uart_duplex_t* state = context;
    return nx_stm32f1_uart_rx_engine_stop(&state->rx, true);
}

static void service(void* context) {
    nx_stm32f1_uart_duplex_t* state = context;
    nx_stm32f1_uart_tx_engine_service(&state->tx);
    nx_stm32f1_uart_rx_engine_service(&state->rx, true, &state->tx.closing);
}

static nx_result_t stop(void* context) {
    nx_stm32f1_uart_duplex_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->tx.config == NULL)
        return NX_SUCCESS;
    state->tx.closing = true;
    if (state->tx.active != NULL) {
        (void)nx_stm32f1_uart_tx_engine_cancel(&state->tx, state->tx.active);
        nx_stm32f1_uart_tx_engine_service(&state->tx);
    }
    const nx_result_t rx = nx_stm32f1_uart_rx_engine_close(&state->rx, true);
    if (rx != NX_SUCCESS)
        return rx;
    return nx_stm32f1_uart_tx_engine_stop(&state->tx);
}

nx_result_t nx_stm32f1_uart_duplex_close(nx_stm32f1_uart_duplex_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    const nx_result_t stopped = stop(state);
    if (stopped != NX_SUCCESS || state->tx.config == NULL)
        return stopped;
    const nx_result_t closed = nx_stm32f1_uart_dma_close(&state->tx);
    if (closed == NX_SUCCESS)
        *state = (nx_stm32f1_uart_duplex_t){0};
    return closed;
}

/** \brief Consume only status IRQ facts; RX DR is drained in task context. */
void nx_stm32f1_uart_duplex_usart_irq(nx_stm32f1_uart_duplex_t* state) {
    nx_stm32f1_uart_rx_engine_irq(&state->rx, true);
    if (state->tx.uart.live && (RCC->APB1ENR & RCC_APB1ENR_USART2EN) != 0U &&
        (USART2->CR1 & USART_CR1_TCIE) != 0U)
        nx_stm32f1_uart_dma_irq(&state->tx);
}

void nx_stm32f1_uart_duplex_tx_irq(nx_stm32f1_uart_duplex_t* state) {
    nx_stm32f1_uart_dma_irq(&state->tx);
}

void nx_stm32f1_uart_duplex_rx_irq(nx_stm32f1_uart_duplex_t* state) {
    nx_stm32f1_uart_rx_engine_irq(&state->rx, true);
}

const nx_uart_ops_t nx_stm32f1_uart_duplex_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .stop = stop,
    .rx_start = rx_start,
    .rx_stop = rx_stop,
};
