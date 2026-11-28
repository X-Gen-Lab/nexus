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

#if defined(NEXUS_STM32F1_UART_DUPLEX)
#define RX_ENGINE
#else
#define RX_ENGINE static
#endif

static bool lane_available(void) {
    const DMA_Channel_TypeDef* lane = DMA1_Channel6;
    return lane->CCR == 0U && lane->CNDTR == 0U && lane->CPAR == 0U &&
           lane->CMAR == 0U && (DMA1->ISR & DMA_EVENTS) == 0U;
}

RX_ENGINE nx_result_t nx_stm32f1_uart_rx_engine_admit(void) {
    return vector_idle(DMA1_Channel6_IRQn) ? NX_SUCCESS : NX_ERROR_BUSY;
}

RX_ENGINE nx_result_t nx_stm32f1_uart_rx_engine_prepare(
    nx_stm32f1_uart_rx_engine_t* rx, uint8_t priority) {
    if (!source_clocks() || !lane_available() ||
        !vector_idle(DMA1_Channel6_IRQn))
        return NX_ERROR_BUSY;
    rx->lane_owned = true;
    rx->previous_priority =
        (uint8_t)NX_STM32F1_IRQ_PRIORITY(DMA1_Channel6_IRQn);
    rx->vector_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(DMA1_Channel6_IRQn, priority);
    if (NX_STM32F1_IRQ_PRIORITY(DMA1_Channel6_IRQn) != priority)
        return NX_ERROR_IO;
    rx->live = true;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32f1_uart_rx_dma_admit(const nx_stm32f1_uart_rx_dma_t* state,
                             const nx_stm32f1_uart_rx_dma_config_t* config) {
    if (state == NULL || config == NULL || config->dma != DMA1 ||
        config->channel != DMA1_Channel6 || config->irq != DMA1_Channel6_IRQn ||
        config->uart.profile != NX_UART_RX_BLOCKS ||
        config->uart.tx_registers != NULL ||
        config->uart.clock_hz != 8000000U || config->regions == NULL ||
        config->region_count == 0U || config->region_count > 16U)
        return NX_ERROR_INVALID;
    if (state->config != NULL)
        return NX_ERROR_STATE;
    const nx_result_t result =
        nx_stm32f1_uart_admit(&state->uart, &config->uart);
    if (result != NX_SUCCESS)
        return result;
    return nx_stm32f1_uart_rx_engine_admit();
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
    if (!lane_available())
        return NX_ERROR_BUSY;
    result = nx_stm32f1_uart_prepare(&state->uart, &config->uart);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_IRQ_DISABLE(USART2_IRQn);
    nx_arch_dsb();
    if (!vector_idle(USART2_IRQn))
        return NX_ERROR_IO;
    return nx_stm32f1_uart_rx_engine_prepare(&state->rx, config->uart.priority);
}

/** \brief Disable new USART requests before asking DMA to retire the loan. */
static bool detach(nx_stm32f1_uart_rx_engine_t* state, bool shared_vector) {
    USART_TypeDef* uart = USART2;
    DMA_Channel_TypeDef* lane = DMA1_Channel6;
    NX_STM32F1_IRQ_DISABLE(DMA1_Channel6_IRQn);
    if (!shared_vector)
        NX_STM32F1_IRQ_DISABLE(USART2_IRQn);
    const bool request_off =
        (uart->CR3 & UART_DMA_CONTROL) == 0U ||
        nx_stm32f1_write_masked(&uart->CR3, UART_DMA_CONTROL, 0U);
    const bool receive_off =
        (uart->CR1 & UART_IRQ_CONTROL) == 0U ||
        nx_stm32f1_write_masked(&uart->CR1, UART_IRQ_CONTROL, 0U);
    const bool engine_off =
        (lane->CCR & DMA_CONTROL) == 0U ||
        nx_stm32f1_write_masked(&lane->CCR, DMA_CONTROL, 0U);
    /* A stuck receiver IRQ can starve the shared executor. Quarantine the
     * vector only on that fatal condition; until close, tasks may still drain.
     */
    if (shared_vector && ((uart->CR1 & USART_CR1_IDLEIE) != 0U ||
                          (uart->CR3 & USART_CR3_EIE) != 0U)) {
        NX_STM32F1_IRQ_DISABLE(USART2_IRQn);
        state->closing = true;
    }
    nx_arch_dsb();
    return request_off && receive_off && engine_off &&
           !NX_STM32F1_IRQ_ENABLED(DMA1_Channel6_IRQn) &&
           (shared_vector || !NX_STM32F1_IRQ_ENABLED(USART2_IRQn));
}

/** \brief Withdraw retired DMA addresses while preserving the count receipt. */
static bool clear_lane_addresses(DMA_Channel_TypeDef* lane) {
    NX_STM32F1_WRITE(&lane->CPAR, 0U);
    NX_STM32F1_WRITE(&lane->CMAR, 0U);
    nx_arch_dsb();
    return lane->CPAR == 0U && lane->CMAR == 0U;
}

/** \brief ES096 2.4.1 excludes CGIF and leaving any live pending publisher. */
static bool retire_sources(bool shared_vector) {
    NX_STM32F1_WRITE(&DMA1->IFCR, DMA_ACK);
    NX_STM32F1_IRQ_CLEAR(DMA1_Channel6_IRQn);
    if (!shared_vector)
        NX_STM32F1_IRQ_CLEAR(USART2_IRQn);
    nx_arch_dsb();
    return (DMA1->ISR & DMA_EVENTS) == 0U && vector_idle(DMA1_Channel6_IRQn) &&
           (shared_vector || vector_idle(USART2_IRQn)) &&
           clear_lane_addresses(DMA1_Channel6);
}

static void observe_boundary(nx_stm32f1_uart_rx_engine_t* state,
                             bool shared_vector) {
    if (state->pending || state->fill.data == NULL || !source_clocks())
        return;
    const uint32_t status = NX_STM32F1_UART_STATUS(USART2);
    const uint32_t events = DMA1->ISR;
    if ((status & (USART_SR_IDLE | UART_ERRORS)) == 0U &&
        (events & (DMA_ISR_TCIF6 | DMA_ISR_TEIF6)) == 0U)
        return;
    state->boundary =
        (status & USART_SR_IDLE) != 0U ? NX_STREAM_BOUNDARY_IDLE : 0U;
    state->fault =
        (status & UART_ERRORS) != 0U || (events & DMA_ISR_TEIF6) != 0U;
    state->pending = true;
    (void)detach(state, shared_vector);
}

/** \brief IRQ only retires hardware and records facts, task publishes. */
RX_ENGINE void nx_stm32f1_uart_rx_engine_irq(nx_stm32f1_uart_rx_engine_t* state,
                                             bool shared_vector) {
    if (state == NULL || !state->live)
        return;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe_boundary(state, shared_vector);
    nx_arch_irq_restore(guard);
}

/** \brief Read DR only after the peripheral request, receiver and DMA stop. */
static nx_result_t finish(nx_stm32f1_uart_rx_engine_t* state,
                          bool shared_vector) {
    if (!state->pending)
        return NX_SUCCESS;
    if (!source_clocks() || !detach(state, shared_vector) ||
        NX_STM32F1_IRQ_ACTIVE(DMA1_Channel6_IRQn) ||
        NX_STM32F1_IRQ_ACTIVE(USART2_IRQn))
        return NX_ERROR_BUSY;

    const uint32_t dma = DMA1->ISR;
    const size_t remaining = DMA1_Channel6->CNDTR;
    USART_TypeDef* uart = USART2;
    const uint32_t status = NX_STM32F1_UART_STATUS(uart);
    if ((status & UART_FACTS) != 0U)
        (void)NX_STM32F1_UART_RECEIVE(uart);
    state->fault = state->fault ||
                   (status & (UART_ERRORS | USART_SR_RXNE)) != 0U ||
                   (dma & DMA_ISR_TEIF6) != 0U ||
                   ((dma & DMA_ISR_TCIF6) != 0U && remaining != 0U) ||
                   remaining > state->fill.capacity;
    if ((NX_STM32F1_UART_STATUS(uart) & UART_FACTS) != 0U ||
        !retire_sources(shared_vector))
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
static bool arm(const nx_stm32f1_uart_rx_engine_t* state, bool shared_vector) {
    DMA_Channel_TypeDef* lane = DMA1_Channel6;
    USART_TypeDef* uart = USART2;
    if (!source_clocks() || !vector_idle(DMA1_Channel6_IRQn) ||
        (!shared_vector && !vector_idle(USART2_IRQn)) ||
        (shared_vector && !NX_STM32F1_IRQ_ENABLED(USART2_IRQn)) ||
        (lane->CCR & DMA_CONTROL) != 0U ||
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
    NX_STM32F1_IRQ_ENABLE(DMA1_Channel6_IRQn);
    if (!shared_vector)
        NX_STM32F1_IRQ_ENABLE(USART2_IRQn);
    nx_arch_dsb();
    return NX_STM32F1_IRQ_ENABLED(DMA1_Channel6_IRQn) &&
           NX_STM32F1_IRQ_ENABLED(USART2_IRQn) &&
           nx_stm32f1_write_masked(&uart->CR3, UART_DMA_CONTROL,
                                   UART_DMA_CONTROL) &&
           nx_stm32f1_write_masked(&uart->CR1, UART_IRQ_CONTROL,
                                   UART_IRQ_CONTROL);
}

static nx_result_t reserve(nx_stm32f1_uart_rx_engine_t* state,
                           bool shared_vector) {
    nx_result_t result = nx_stream_reserve(state->stream, &state->fill);
    if (result != NX_SUCCESS)
        return result;
    if (!arm(state, shared_vector)) {
        /* The accepted loan remains until both sources and IRQs retire. */
        state->fault = true;
        state->pending = true;
        if (source_clocks())
            (void)detach(state, shared_vector);
    }
    return NX_SUCCESS;
}

#if defined(NEXUS_STM32F1_UART_DUPLEX)
/** \brief A whole receive pool remains reserved, including consumer loans. */
bool nx_stm32f1_uart_stream_contains(const nx_stream_t* stream,
                                     const void* address, size_t bytes) {
    if (stream == NULL)
        return false;
    if (nx_io_dispatch_overlap(address, bytes, stream, sizeof(*stream)) ||
        nx_io_dispatch_overlap(address, bytes, stream->slots,
                               stream->count * sizeof(*stream->slots)))
        return true;
    for (size_t i = 0U; i < stream->count; ++i) {
        if (nx_io_dispatch_overlap(address, bytes, stream->slots[i].data,
                                   stream->slots[i].capacity))
            return true;
    }
    return false;
}
#endif

static bool independent(const nx_stm32f1_uart_rx_storage_t* storage,
                        const void* addr, size_t size) {
    return !nx_io_dispatch_overlap(addr, size, storage->owner,
                                   storage->owner_size) &&
           !nx_io_dispatch_overlap(addr, size, storage->config,
                                   storage->config_size) &&
           !nx_io_dispatch_overlap(addr, size, storage->regions,
                                   storage->region_count *
                                       sizeof(*storage->regions));
}

static nx_result_t validate_stream(const nx_stm32f1_uart_rx_storage_t* storage,
                                   const nx_stream_t* stream) {
    if (!nx_io_dispatch_span(stream, sizeof(*stream), _Alignof(nx_stream_t)) ||
        !independent(storage, stream, sizeof(*stream)) || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) || stream->stopping)
        return NX_ERROR_INVALID;
    const size_t slots_bytes = stream->count * sizeof(*stream->slots);
    if (!nx_io_dispatch_span(stream->slots, slots_bytes,
                             _Alignof(nx_stream_slot_t)) ||
        !independent(storage, stream->slots, slots_bytes) ||
        nx_io_dispatch_overlap(stream, sizeof(*stream), stream->slots,
                               slots_bytes))
        return NX_ERROR_INVALID;
    if (stream->outstanding != 0U || !nx_stream_can_reserve(stream))
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->capacity > UINT16_MAX ||
            !independent(storage, slot->data, slot->capacity))
            return NX_ERROR_INVALID;
        const nx_result_t result = nx_dma_buffer_validate(
            storage->regions, storage->region_count, slot->data, slot->capacity,
            1U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
#if defined(NEXUS_STM32F1_UART_DUPLEX)
    if (storage->tx != NULL &&
        (nx_stm32f1_uart_stream_contains(stream, storage->tx,
                                         sizeof(*storage->tx)) ||
         nx_stm32f1_uart_stream_contains(stream, storage->tx->data,
                                         storage->tx->length)))
        return NX_ERROR_INVALID;
#endif
    return NX_SUCCESS;
}

RX_ENGINE nx_result_t nx_stm32f1_uart_rx_engine_start(
    nx_stm32f1_uart_rx_engine_t* state, nx_stream_t* stream,
    const nx_stm32f1_uart_rx_storage_t* storage, bool shared_vector) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!state->live || state->closing || !source_clocks())
        return NX_ERROR_STATE;
    if (state->stream != NULL)
        return NX_ERROR_BUSY;
    const nx_result_t valid = validate_stream(storage, stream);
    if (valid != NX_SUCCESS)
        return valid;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
#if defined(NEXUS_STM32F1_UART_DUPLEX)
    /* Recheck shared shutdown facts after the unmasked span walk. */
    if (shared_vector && (state->closing || *storage->peer_closing ||
                          !NX_STM32F1_IRQ_ENABLED(USART2_IRQn))) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
#endif
    state->stream = stream;
    const nx_result_t admitted = reserve(state, shared_vector);
    if (admitted != NX_SUCCESS)
        state->stream = NULL;
    nx_arch_irq_restore(guard);
    return admitted;
}

RX_ENGINE void
nx_stm32f1_uart_rx_engine_service(nx_stm32f1_uart_rx_engine_t* state,
                                  bool shared_vector,
                                  const bool* peer_closing) {
    if (!nx_stm32f1_cold_context() || state->stream == NULL)
        return;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
#if defined(NEXUS_STM32F1_UART_DUPLEX)
    if (shared_vector &&
        (*peer_closing || !NX_STM32F1_IRQ_ENABLED(USART2_IRQn)))
        state->closing = true;
#else
    (void)peer_closing;
#endif
    observe_boundary(state, shared_vector);
    if (finish(state, shared_vector) == NX_SUCCESS &&
        state->fill.data == NULL && !state->closing &&
        nx_stream_can_reserve(state->stream))
        (void)reserve(state, shared_vector);
    nx_arch_irq_restore(guard);
}

RX_ENGINE nx_result_t nx_stm32f1_uart_rx_engine_stop(
    nx_stm32f1_uart_rx_engine_t* state, bool shared_vector) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->stream == NULL)
        return NX_SUCCESS;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    (void)nx_stream_stop(state->stream);
    if (state->fill.data != NULL) {
        observe_boundary(state, shared_vector);
        state->pending = true;
    }
    nx_result_t result = finish(state, shared_vector);
    if (result == NX_SUCCESS) {
        result = nx_stream_stop(state->stream);
        if (result == NX_SUCCESS)
            state->stream = NULL;
    }
    nx_arch_irq_restore(guard);
    return result;
}

/** \brief Withdraw the fixed lane and vector independently of USART2. */
RX_ENGINE nx_result_t nx_stm32f1_uart_rx_engine_close(
    nx_stm32f1_uart_rx_engine_t* rx, bool shared_vector) {
    rx->closing = true;
    const nx_result_t result =
        nx_stm32f1_uart_rx_engine_stop(rx, shared_vector);
    if (result != NX_SUCCESS)
        return result;
    if (rx->vector_owned) {
        NX_STM32F1_IRQ_DISABLE(DMA1_Channel6_IRQn);
        NX_STM32F1_IRQ_SET_PRIORITY(DMA1_Channel6_IRQn, rx->previous_priority);
        nx_arch_dsb();
        if (!vector_idle(DMA1_Channel6_IRQn) ||
            NX_STM32F1_IRQ_PRIORITY(DMA1_Channel6_IRQn) !=
                rx->previous_priority)
            return NX_ERROR_IO;
        rx->vector_owned = false;
    }
    if (rx->lane_owned) {
        DMA_Channel_TypeDef* lane = DMA1_Channel6;
        if (!dma_clock() || (lane->CCR & DMA_CONTROL) != 0U ||
            !nx_stm32f1_write_masked(&lane->CCR, UINT16_MAX, 0U) ||
            !nx_stm32f1_write_masked(&lane->CNDTR, UINT16_MAX, 0U) ||
            !clear_lane_addresses(lane))
            return NX_ERROR_IO;
        rx->lane_owned = false;
    }
    rx->live = false;
    return NX_SUCCESS;
}

void nx_stm32f1_uart_rx_dma_irq(nx_stm32f1_uart_rx_dma_t* state) {
    if (state != NULL)
        nx_stm32f1_uart_rx_engine_irq(&state->rx, false);
}

static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    if (state->config == NULL)
        return NX_ERROR_STATE;
    const nx_stm32f1_uart_rx_storage_t storage = {
        .regions = state->config->regions,
        .region_count = state->config->region_count,
        .owner = state,
        .owner_size = sizeof(*state),
        .config = state->config,
        .config_size = sizeof(*state->config),
    };
    return nx_stm32f1_uart_rx_engine_start(&state->rx, stream, &storage, false);
}

static void service(void* context) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    nx_stm32f1_uart_rx_engine_service(&state->rx, false, NULL);
}

static nx_result_t rx_stop(void* context) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    return nx_stm32f1_uart_rx_engine_stop(&state->rx, false);
}

static nx_result_t stop(void* context) {
    nx_stm32f1_uart_rx_dma_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    const nx_result_t result =
        nx_stm32f1_uart_rx_engine_close(&state->rx, false);
    return result == NX_SUCCESS ? nx_stm32f1_uart_stop(&state->uart) : result;
}

nx_result_t nx_stm32f1_uart_rx_dma_close(nx_stm32f1_uart_rx_dma_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    const nx_result_t stopped = stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL)
        return stopped;
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
