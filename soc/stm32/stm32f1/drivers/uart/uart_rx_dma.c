/**
 * \file            uart_rx_dma.c
 * \brief           F103 USART2 finite byte blocks on DMA1 Channel 6
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "uart_rx_dma.h"
#include "dispatch_checks.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_usart.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "USART2 RX DMA is reviewed for the STM32F103xB startup profile"
#endif

#ifndef NX_STM32F1_UART_STATUS
#define NX_STM32F1_UART_STATUS(reg)  ((reg)->SR)
#define NX_STM32F1_UART_RECEIVE(reg) LL_USART_ReceiveData8(reg)
#endif

#ifndef NX_STM32F1_IRQ_ENABLED
#define NX_STM32F1_IRQ_ENABLED(irq)           NVIC_GetEnableIRQ(irq)
#define NX_STM32F1_IRQ_PENDING(irq)           NVIC_GetPendingIRQ(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)            NVIC_GetActive(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)          NVIC_GetPriority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, val) NVIC_SetPriority((irq), (val))
#define NX_STM32F1_IRQ_ENABLE(irq)            NVIC_EnableIRQ(irq)
#define NX_STM32F1_IRQ_DISABLE(irq)           NVIC_DisableIRQ(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)             NVIC_ClearPendingIRQ(irq)
#endif

#define UART_ERRORS                                                            \
    ((uint32_t)(USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE))
#define UART_FACTS       (UART_ERRORS | USART_SR_IDLE | USART_SR_RXNE)
#define UART_IRQ_CONTROL ((uint32_t)(USART_CR1_RE | USART_CR1_IDLEIE))
#define UART_DMA_CONTROL ((uint32_t)(USART_CR3_DMAR | USART_CR3_EIE))
#define DMA_CONTROL                                                            \
    ((uint32_t)(DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE))
#define DMA_MODE   ((uint32_t)(DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE))
#define DMA_EVENTS ((uint32_t)(DMA_ISR_TCIF6 | DMA_ISR_HTIF6 | DMA_ISR_TEIF6))
#define DMA_ACK                                                                \
    ((uint32_t)(DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6))

static bool dma_clock(void) {
    return (RCC->AHBENR & RCC_AHBENR_DMA1EN) != 0U;
}

static bool source_clocks(void) {
    return dma_clock() && (RCC->APB1ENR & RCC_APB1ENR_USART2EN) != 0U;
}

static bool vector_idle(IRQn_Type irq) {
    return !NX_STM32F1_IRQ_ENABLED(irq) && !NX_STM32F1_IRQ_PENDING(irq) &&
           !NX_STM32F1_IRQ_ACTIVE(irq);
}

nx_result_t
nx_stm32f1_uart_rx_dma_admit(const nx_stm32f1_uart_rx_dma_t* state,
                             const nx_stm32f1_uart_rx_dma_config_t* config) {
    if (state == NULL || config == NULL || config->dma != DMA1 ||
        config->channel != DMA1_Channel6 || config->irq != DMA1_Channel6_IRQn ||
        config->uart.profile != NX_UART_RX_BLOCKS ||
        config->uart.clock_hz != 8000000U || config->regions == NULL ||
        config->region_count == 0U || config->region_count > 16U)
        return NX_ERROR_INVALID;
    if (state->config != NULL)
        return NX_ERROR_STATE;
    const nx_result_t result =
        nx_stm32f1_uart_admit(&state->uart, &config->uart);
    if (result != NX_SUCCESS)
        return result;
    return vector_idle(config->irq) ? NX_SUCCESS : NX_ERROR_BUSY;
}

nx_result_t
nx_stm32f1_uart_rx_dma_prepare(nx_stm32f1_uart_rx_dma_t* state,
                               const nx_stm32f1_uart_rx_dma_config_t* config) {
    nx_result_t result = nx_stm32f1_uart_rx_dma_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    state->config = config;
    state->gate_added = !dma_clock();
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN,
                                 RCC_AHBENR_DMA1EN))
        return NX_ERROR_IO;
    const DMA_Channel_TypeDef* lane = config->channel;
    if (lane->CCR != 0U || lane->CNDTR != 0U || lane->CPAR != 0U ||
        lane->CMAR != 0U || (config->dma->ISR & DMA_EVENTS) != 0U)
        return NX_ERROR_BUSY;
    state->lane_owned = true;
    result = nx_stm32f1_uart_prepare(&state->uart, &config->uart);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_IRQ_DISABLE(config->uart.irq);
    nx_arch_dsb();
    if (!vector_idle(config->uart.irq))
        return NX_ERROR_IO;
    state->previous_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(config->irq);
    state->vector_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(config->irq, config->uart.priority);
    if (NX_STM32F1_IRQ_PRIORITY(config->irq) != config->uart.priority)
        return NX_ERROR_IO;
    state->live = true;
    return NX_SUCCESS;
}

/** \brief Disable new USART requests before asking DMA to retire the loan. */
static bool detach(const nx_stm32f1_uart_rx_dma_config_t* config) {
    USART_TypeDef* uart = config->uart.registers;
    DMA_Channel_TypeDef* lane = config->channel;
    NX_STM32F1_IRQ_DISABLE(config->irq);
    NX_STM32F1_IRQ_DISABLE(config->uart.irq);
    const bool request_off =
        (uart->CR3 & UART_DMA_CONTROL) == 0U ||
        nx_stm32f1_write_masked(&uart->CR3, UART_DMA_CONTROL, 0U);
    const bool receive_off =
        (uart->CR1 & UART_IRQ_CONTROL) == 0U ||
        nx_stm32f1_write_masked(&uart->CR1, UART_IRQ_CONTROL, 0U);
    const bool engine_off =
        (lane->CCR & DMA_CONTROL) == 0U ||
        nx_stm32f1_write_masked(&lane->CCR, DMA_CONTROL, 0U);
    nx_arch_dsb();
    return request_off && receive_off && engine_off &&
           !NX_STM32F1_IRQ_ENABLED(config->irq) &&
           !NX_STM32F1_IRQ_ENABLED(config->uart.irq);
}

/** \brief ES096 2.4.1 excludes CGIF and leaving any live pending publisher. */
static bool retire_sources(const nx_stm32f1_uart_rx_dma_config_t* config) {
    NX_STM32F1_WRITE(&config->dma->IFCR, DMA_ACK);
    NX_STM32F1_IRQ_CLEAR(config->irq);
    NX_STM32F1_IRQ_CLEAR(config->uart.irq);
    nx_arch_dsb();
    return (config->dma->ISR & DMA_EVENTS) == 0U && vector_idle(config->irq) &&
           vector_idle(config->uart.irq);
}

static void observe_boundary(nx_stm32f1_uart_rx_dma_t* state) {
    if (state->pending || state->fill.data == NULL || !source_clocks())
        return;
    const nx_stm32f1_uart_rx_dma_config_t* config = state->config;
    const uint32_t status = NX_STM32F1_UART_STATUS(config->uart.registers);
    const uint32_t events = config->dma->ISR;
    if ((status & (USART_SR_IDLE | UART_ERRORS)) == 0U &&
        (events & (DMA_ISR_TCIF6 | DMA_ISR_TEIF6)) == 0U)
        return;
    state->boundary =
        (status & USART_SR_IDLE) != 0U ? NX_STREAM_BOUNDARY_IDLE : 0U;
    state->fault =
        (status & UART_ERRORS) != 0U || (events & DMA_ISR_TEIF6) != 0U;
    state->pending = true;
    (void)detach(config);
}

/** \brief IRQ only retires hardware and records facts, task publishes. */
void nx_stm32f1_uart_rx_dma_irq(nx_stm32f1_uart_rx_dma_t* state) {
    if (state == NULL || !state->live)
        return;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe_boundary(state);
    nx_arch_irq_restore(guard);
}

/** \brief Read DR only after the peripheral request, receiver and DMA stop. */
static nx_result_t finish(nx_stm32f1_uart_rx_dma_t* state) {
    if (!state->pending)
        return NX_SUCCESS;
    const nx_stm32f1_uart_rx_dma_config_t* config = state->config;
    if (!source_clocks() || !detach(config) ||
        NX_STM32F1_IRQ_ACTIVE(config->irq) ||
        NX_STM32F1_IRQ_ACTIVE(config->uart.irq))
        return NX_ERROR_BUSY;

    const uint32_t dma = config->dma->ISR;
    const size_t remaining = config->channel->CNDTR;
    USART_TypeDef* uart = config->uart.registers;
    const uint32_t status = NX_STM32F1_UART_STATUS(uart);
    if ((status & UART_FACTS) != 0U)
        (void)NX_STM32F1_UART_RECEIVE(uart);
    state->fault = state->fault ||
                   (status & (UART_ERRORS | USART_SR_RXNE)) != 0U ||
                   (dma & DMA_ISR_TEIF6) != 0U ||
                   ((dma & DMA_ISR_TCIF6) != 0U && remaining != 0U) ||
                   remaining > state->fill.capacity;
    if ((NX_STM32F1_UART_STATUS(uart) & UART_FACTS) != 0U ||
        !retire_sources(config))
        return NX_ERROR_BUSY;

    nx_result_t result;
    if (state->fault || remaining == state->fill.capacity) {
        result = nx_stream_abort(state->stream, &state->fill, true);
        if (result == NX_SUCCESS && state->fault)
            nx_stream_note_loss(state->stream, 1U);
    } else {
        result = nx_stream_publish(state->stream, &state->fill,
                                   state->fill.capacity - remaining,
                                   state->boundary);
    }
    if (result == NX_SUCCESS) {
        state->fill = (nx_stream_fill_t){0};
        state->pending = false;
        state->fault = false;
        state->boundary = 0U;
    }
    return result;
}

/** \brief Reversible arm prepares one NORMAL byte DMA target. */
static bool arm(const nx_stm32f1_uart_rx_dma_t* state) {
    const nx_stm32f1_uart_rx_dma_config_t* config = state->config;
    DMA_Channel_TypeDef* lane = config->channel;
    USART_TypeDef* uart = config->uart.registers;
    if (!source_clocks() || !vector_idle(config->irq) ||
        !vector_idle(config->uart.irq) || (lane->CCR & DMA_CONTROL) != 0U ||
        (uart->CR3 & UART_DMA_CONTROL) != 0U ||
        (uart->CR1 & UART_IRQ_CONTROL) != 0U)
        return false;

    const uint32_t peripheral = (uint32_t)(uintptr_t)&uart->DR;
    const uint32_t memory = (uint32_t)(uintptr_t)state->fill.data;
    const uint32_t count = (uint32_t)state->fill.capacity;
    if (!nx_stm32f1_write_masked(&lane->CPAR, UINT32_MAX, peripheral) ||
        !nx_stm32f1_write_masked(&lane->CMAR, UINT32_MAX, memory) ||
        !nx_stm32f1_write_masked(&lane->CNDTR, UINT16_MAX, count) ||
        !nx_stm32f1_write_masked(&lane->CCR, UINT16_MAX, DMA_MODE | DMA_CCR_EN))
        return false;
    NX_STM32F1_IRQ_ENABLE(config->irq);
    NX_STM32F1_IRQ_ENABLE(config->uart.irq);
    nx_arch_dsb();
    return NX_STM32F1_IRQ_ENABLED(config->irq) &&
           NX_STM32F1_IRQ_ENABLED(config->uart.irq) &&
           nx_stm32f1_write_masked(&uart->CR3, UART_DMA_CONTROL,
                                   UART_DMA_CONTROL) &&
           nx_stm32f1_write_masked(&uart->CR1, UART_IRQ_CONTROL,
                                   UART_IRQ_CONTROL);
}

static nx_result_t reserve(nx_stm32f1_uart_rx_dma_t* state) {
    nx_result_t result = nx_stream_reserve(state->stream, &state->fill);
    if (result != NX_SUCCESS)
        return result;
    if (!arm(state)) {
        /* The accepted loan remains until both sources and IRQs retire. */
        state->fault = true;
        state->pending = true;
        if (source_clocks())
            (void)detach(state->config);
    }
    return NX_SUCCESS;
}

static bool independent(const nx_stm32f1_uart_rx_dma_t* state, const void* addr,
                        size_t size) {
    return !nx_io_dispatch_overlap(addr, size, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(addr, size, state->config,
                                   sizeof(*state->config)) &&
           !nx_io_dispatch_overlap(addr, size, state->config->regions,
                                   state->config->region_count *
                                       sizeof(*state->config->regions));
}

static nx_result_t validate_stream(const nx_stm32f1_uart_rx_dma_t* state,
                                   const nx_stream_t* stream) {
    if (!nx_io_dispatch_span(stream, sizeof(*stream), _Alignof(nx_stream_t)) ||
        !independent(state, stream, sizeof(*stream)) || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) || stream->stopping)
        return NX_ERROR_INVALID;
    const size_t slots_bytes = stream->count * sizeof(*stream->slots);
    if (!nx_io_dispatch_span(stream->slots, slots_bytes,
                             _Alignof(nx_stream_slot_t)) ||
        !independent(state, stream->slots, slots_bytes) ||
        nx_io_dispatch_overlap(stream, sizeof(*stream), stream->slots,
                               slots_bytes))
        return NX_ERROR_INVALID;
    if (stream->outstanding != 0U || !nx_stream_can_reserve(stream))
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->capacity > UINT16_MAX ||
            !independent(state, slot->data, slot->capacity))
            return NX_ERROR_INVALID;
        const nx_result_t result = nx_dma_buffer_validate(
            state->config->regions, state->config->region_count, slot->data,
            slot->capacity, 1U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}

static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!state->live || state->closing || !source_clocks())
        return NX_ERROR_STATE;
    if (state->stream != NULL)
        return NX_ERROR_BUSY;
    const nx_result_t valid = validate_stream(state, stream);
    if (valid != NX_SUCCESS)
        return valid;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    state->stream = stream;
    const nx_result_t admitted = reserve(state);
    if (admitted != NX_SUCCESS)
        state->stream = NULL;
    nx_arch_irq_restore(guard);
    return admitted;
}

static void service(void* context) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    if (!nx_stm32f1_cold_context() || state->stream == NULL)
        return;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe_boundary(state);
    if (finish(state) == NX_SUCCESS && state->fill.data == NULL &&
        !state->closing && nx_stream_can_reserve(state->stream))
        (void)reserve(state);
    nx_arch_irq_restore(guard);
}

static nx_result_t rx_stop(void* context) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->stream == NULL)
        return NX_SUCCESS;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    (void)nx_stream_stop(state->stream);
    if (state->fill.data != NULL) {
        observe_boundary(state);
        state->pending = true;
    }
    nx_result_t result = finish(state);
    if (result == NX_SUCCESS) {
        result = nx_stream_stop(state->stream);
        if (result == NX_SUCCESS)
            state->stream = NULL;
    }
    nx_arch_irq_restore(guard);
    return result;
}

static nx_result_t stop(void* context) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    state->closing = true;
    const nx_result_t result = rx_stop(state);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32f1_uart_rx_dma_config_t* config = state->config;
    if (state->vector_owned) {
        NX_STM32F1_IRQ_DISABLE(config->irq);
        NX_STM32F1_IRQ_SET_PRIORITY(config->irq, state->previous_priority);
        nx_arch_dsb();
        if (!vector_idle(config->irq) ||
            NX_STM32F1_IRQ_PRIORITY(config->irq) != state->previous_priority)
            return NX_ERROR_IO;
        state->vector_owned = false;
    }
    state->live = false;
    return nx_stm32f1_uart_stop(&state->uart);
}

nx_result_t nx_stm32f1_uart_rx_dma_close(nx_stm32f1_uart_rx_dma_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    const nx_result_t stopped = stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL)
        return stopped;
    if (state->lane_owned) {
        DMA_Channel_TypeDef* lane = state->config->channel;
        if (!dma_clock() || (lane->CCR & DMA_CONTROL) != 0U ||
            !nx_stm32f1_write_masked(&lane->CCR, UINT16_MAX, 0U) ||
            !nx_stm32f1_write_masked(&lane->CNDTR, UINT16_MAX, 0U) ||
            !nx_stm32f1_write_masked(&lane->CPAR, UINT32_MAX, 0U) ||
            !nx_stm32f1_write_masked(&lane->CMAR, UINT32_MAX, 0U))
            return NX_ERROR_IO;
        state->lane_owned = false;
    }
    const nx_result_t restored = nx_stm32f1_uart_close(&state->uart);
    if (restored != NX_SUCCESS)
        return restored;
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN, 0U))
        return NX_ERROR_IO;
    *state = (nx_stm32f1_uart_rx_dma_t){0};
    return NX_SUCCESS;
}

const nx_uart_ops_t nx_stm32f1_uart_rx_dma_ops = {
    .service = service, .stop = stop, .rx_start = rx_start, .rx_stop = rx_stop};
