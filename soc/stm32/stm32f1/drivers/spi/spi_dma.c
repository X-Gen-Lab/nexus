/**
 * \file            spi_dma.c
 * \brief           F103 SPI2 finite DMA with explicit width and wire withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "spi_dma.h"
#include "dispatch_checks.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_spi.h"
#include "system/platform.h"
#if defined(NX_STM32F1_SPI_DMA_SEQUENCE)
#include "spi_dma_sequence_backend.h"
#endif

#if !defined(STM32F103xB)
#error "SPI2 finite DMA is reviewed for the STM32F103xB startup profile"
#endif

#ifndef NX_STM32F1_SPI_STATUS
#define NX_STM32F1_SPI_STATUS(reg) ((reg)->SR)
#define NX_STM32F1_SPI_RECEIVE(reg, bits)                                      \
    ((bits) == 16U ? LL_SPI_ReceiveData16(reg) : LL_SPI_ReceiveData8(reg))
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

#define RX_IRQ DMA1_Channel4_IRQn
#define TX_IRQ DMA1_Channel5_IRQn
#define DMA_ENABLE                                                             \
    ((uint32_t)(DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE))
#define DMA_MODE   ((uint32_t)(DMA_CCR_TCIE | DMA_CCR_TEIE))
#define RX_EVENTS  ((uint32_t)(DMA_ISR_TCIF4 | DMA_ISR_HTIF4 | DMA_ISR_TEIF4))
#define TX_EVENTS  ((uint32_t)(DMA_ISR_TCIF5 | DMA_ISR_HTIF5 | DMA_ISR_TEIF5))
#define DMA_ERRORS ((uint32_t)(DMA_ISR_TEIF4 | DMA_ISR_TEIF5))
#define DMA_DONE   ((uint32_t)(DMA_ISR_TCIF4 | DMA_ISR_TCIF5))
#define DMA_ACK                                                                \
    ((uint32_t)(DMA_IFCR_CTCIF4 | DMA_IFCR_CHTIF4 | DMA_IFCR_CTEIF4 |          \
                DMA_IFCR_CTCIF5 | DMA_IFCR_CHTIF5 | DMA_IFCR_CTEIF5))
#define SPI_REQUESTS ((uint32_t)(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN))
#define SPI_ERRORS   ((uint32_t)(SPI_SR_OVR | SPI_SR_MODF | SPI_SR_CRCERR))
#define SPI_REST     ((uint32_t)(SPI_SR_RXNE | SPI_ERRORS))
#define SPI_IDLE     ((uint32_t)(SPI_SR_TXE | SPI_SR_BSY))
#define SPI_DRAIN_US UINT32_C(700)

static bool dma_clock(void) {
    return (RCC->AHBENR & RCC_AHBENR_DMA1EN) != 0U;
}

static bool source_clocks(void) {
    return dma_clock() && (RCC->APB1ENR & RCC_APB1ENR_SPI2EN) != 0U;
}

static bool vector_idle(IRQn_Type irq) {
    return !NX_STM32F1_IRQ_ENABLED(irq) && !NX_STM32F1_IRQ_PENDING(irq) &&
           !NX_STM32F1_IRQ_ACTIVE(irq);
}

static bool lane_clear(const DMA_Channel_TypeDef* lane) {
    return lane->CCR == 0U && lane->CPAR == 0U && lane->CMAR == 0U &&
           lane->CNDTR == 0U;
}

nx_result_t
nx_stm32f1_spi_dma_admit(const nx_stm32f1_spi_dma_t* state,
                         const nx_stm32f1_spi_dma_config_t* config) {
    if (state == NULL || config == NULL || config->spi.registers != SPI2 ||
        config->spi.clock_hz != 8000000U || config->dma != DMA1 ||
        config->rx != DMA1_Channel4 || config->tx != DMA1_Channel5 ||
        config->regions == NULL || config->region_count == 0U ||
        config->region_count > 16U ||
        config->priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    if (state->config != NULL)
        return NX_ERROR_STATE;
    const nx_result_t result = nx_stm32f1_spi_admit(&state->spi, &config->spi);
    if (result != NX_SUCCESS)
        return result;
    return vector_idle(RX_IRQ) && vector_idle(TX_IRQ) && vector_idle(SPI2_IRQn)
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

nx_result_t
nx_stm32f1_spi_dma_prepare(nx_stm32f1_spi_dma_t* state,
                           const nx_stm32f1_spi_dma_config_t* config) {
    nx_result_t result = nx_stm32f1_spi_dma_admit(state, config);
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
    if (!lane_clear(config->rx) || !lane_clear(config->tx) ||
        (config->dma->ISR & (RX_EVENTS | TX_EVENTS)) != 0U)
        return NX_ERROR_BUSY;
    state->lanes_owned = true;
    result = nx_stm32f1_spi_prepare(&state->spi, &config->spi);
    if (result != NX_SUCCESS)
        return result;
    state->rx_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(RX_IRQ);
    state->tx_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(TX_IRQ);
    state->vectors_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(RX_IRQ, config->priority);
    NX_STM32F1_IRQ_SET_PRIORITY(TX_IRQ, config->priority);
    if (NX_STM32F1_IRQ_PRIORITY(RX_IRQ) != config->priority ||
        NX_STM32F1_IRQ_PRIORITY(TX_IRQ) != config->priority)
        return NX_ERROR_IO;
    state->ready = true;
    return NX_SUCCESS;
}

static bool disable_lane(DMA_Channel_TypeDef* lane) {
    return (lane->CCR & DMA_ENABLE) == 0U ||
           nx_stm32f1_write_masked(&lane->CCR, DMA_ENABLE, 0U);
}

/** \brief No new TX clocks or RX memory writes can begin after withdrawal. */
static bool detach(const nx_stm32f1_spi_dma_config_t* config) {
    NX_STM32F1_IRQ_DISABLE(TX_IRQ);
    NX_STM32F1_IRQ_DISABLE(RX_IRQ);
    SPI_TypeDef* spi = config->spi.registers;
    const bool requests_off =
        (spi->CR2 & SPI_REQUESTS) == 0U ||
        nx_stm32f1_write_masked(&spi->CR2, SPI_REQUESTS, 0U);
    const bool tx_off = disable_lane(config->tx);
    const bool rx_off = disable_lane(config->rx);
    nx_arch_dsb();
    return requests_off && tx_off && rx_off &&
           !NX_STM32F1_IRQ_ENABLED(TX_IRQ) && !NX_STM32F1_IRQ_ENABLED(RX_IRQ);
}

/** \brief One first terminal decision; a successful transfer is still
 * provisional. */
static void drain(nx_stm32f1_spi_dma_t* state, nx_result_t reason,
                  nx_time_us_t now) {
    if (!state->draining) {
        state->terminal = reason;
        state->draining = true;
        if (nx_deadline_finite_after(now, SPI_DRAIN_US,
                                     &state->drain_deadline) != NX_SUCCESS)
            state->drain_deadline = 0U;
        (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    }
    if (source_clocks())
        (void)detach(state->config);
}

/** \brief Observe each terminal cause before the later task-side retirement. */
static void observe(nx_stm32f1_spi_dma_t* state, nx_time_us_t now) {
    if (state->draining)
        return;
    if (!source_clocks()) {
        NX_STM32F1_IRQ_DISABLE(TX_IRQ);
        NX_STM32F1_IRQ_DISABLE(RX_IRQ);
        drain(state, NX_ERROR_IO, now);
        return;
    }
    const uint32_t flags = state->config->dma->ISR;
    const uint32_t status = NX_STM32F1_SPI_STATUS(state->config->spi.registers);
    if ((flags & DMA_ERRORS) != 0U || (status & SPI_ERRORS) != 0U)
        drain(state, NX_ERROR_IO, now);
    else if (nx_deadline_expired(state->active->base.deadline, now))
        drain(state, NX_ERROR_TIMEOUT, now);
    else if ((flags & DMA_DONE) == DMA_DONE)
        drain(state, NX_SUCCESS, now);
    else {
        /* Keep exact flags through retirement; these vectors cannot storm. */
        if ((flags & DMA_ISR_TCIF4) != 0U)
            NX_STM32F1_IRQ_DISABLE(RX_IRQ);
        if ((flags & DMA_ISR_TCIF5) != 0U)
            NX_STM32F1_IRQ_DISABLE(TX_IRQ);
    }
}

void nx_stm32f1_spi_dma_irq(nx_stm32f1_spi_dma_t* state) {
    if (state == NULL || state->active == NULL)
        return;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe(state, nx_time_now_us());
    nx_arch_irq_restore(guard);
}

static bool stage(DMA_Channel_TypeDef* lane, uint32_t peripheral,
                  const void* data, uint32_t length, uint32_t mode) {
    return nx_stm32f1_write_masked(&lane->CPAR, UINT32_MAX, peripheral) &&
           nx_stm32f1_write_masked(&lane->CMAR, UINT32_MAX,
                                   (uint32_t)(uintptr_t)data) &&
           nx_stm32f1_write_masked(&lane->CNDTR, UINT16_MAX, length) &&
           nx_stm32f1_write_masked(&lane->CCR, UINT16_MAX,
                                   DMA_MODE | mode | DMA_CCR_EN);
}

/** \brief Prime RX before enabling SPI and enabling TX requests last. */
static bool arm(nx_stm32f1_spi_dma_t* state, uint8_t word_bits,
                uint8_t memory_flags, bool retained_cs) {
    const nx_stm32f1_spi_dma_config_t* config = state->config;
    const nx_spi_request_t* request = state->active;
    SPI_TypeDef* spi = config->spi.registers;
    const uint32_t peripheral = (uint32_t)(uintptr_t)&spi->DR;
    const uint32_t length = (uint32_t)request->length;
    uint32_t controls = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1;
    uint32_t width = 0U;
    if (word_bits == 16U) {
        controls |= SPI_CR1_DFF;
        width = DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0;
    }
    const uint32_t rx_mode =
        width |
        ((memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U ? 0U : DMA_CCR_MINC);
    const uint32_t tx_mode =
        width | DMA_CCR_DIR |
        ((memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U ? 0U : DMA_CCR_MINC);
    if (!nx_stm32f1_write_masked(&spi->CR1, UINT16_MAX, controls) ||
        !stage(config->rx, peripheral, request->rx, length, rx_mode) ||
        !stage(config->tx, peripheral, request->tx, length, tx_mode) ||
        (!retained_cs && !nx_stm32f1_spi_select_cs(&state->spi, true)))
        return false;
    NX_STM32F1_IRQ_ENABLE(RX_IRQ);
    NX_STM32F1_IRQ_ENABLE(TX_IRQ);
    nx_arch_dsb();
    return NX_STM32F1_IRQ_ENABLED(RX_IRQ) && NX_STM32F1_IRQ_ENABLED(TX_IRQ) &&
           nx_stm32f1_write_masked(&spi->CR2, SPI_REQUESTS, SPI_CR2_RXDMAEN) &&
           nx_stm32f1_write_masked(&spi->CR1, SPI_CR1_SPE, SPI_CR1_SPE) &&
           nx_stm32f1_write_masked(&spi->CR2, SPI_REQUESTS, SPI_REQUESTS);
}

static bool independent(const nx_stm32f1_spi_dma_endpoint_t* endpoint,
                        const void* address, size_t length) {
    const nx_stm32f1_spi_dma_t* state = endpoint->port;
    const nx_stm32f1_spi_dma_config_t* config = state->config;
    return !nx_io_dispatch_overlap(address, length, endpoint,
                                   sizeof(*endpoint)) &&
           !nx_io_dispatch_overlap(address, length, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(address, length, config, sizeof(*config)) &&
           !nx_io_dispatch_overlap(address, length, config->regions,
                                   config->region_count *
                                       sizeof(*config->regions));
}

static nx_result_t validate(const nx_stm32f1_spi_dma_endpoint_t* endpoint,
                            const nx_spi_request_t* request, uint8_t word_bits,
                            uint8_t memory_flags) {
    if (!nx_io_dispatch_span(request, sizeof(*request),
                             _Alignof(nx_spi_request_t)) ||
        !independent(endpoint, request, sizeof(*request)) ||
        request->length == 0U || request->length > UINT16_MAX ||
        request->base.deadline == NX_DEADLINE_NEVER ||
        (word_bits != 8U && word_bits != 16U))
        return NX_ERROR_INVALID;
    const size_t unit = word_bits / 8U;
    const size_t tx_bytes = (memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U
                                ? unit
                                : request->length * unit;
    const size_t rx_bytes = (memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U
                                ? unit
                                : request->length * unit;
    if (!independent(endpoint, request->tx, tx_bytes) ||
        !independent(endpoint, request->rx, rx_bytes) ||
        nx_io_dispatch_overlap(request->tx, tx_bytes, request->rx, rx_bytes) ||
        nx_io_dispatch_overlap(request, sizeof(*request), request->tx,
                               tx_bytes) ||
        nx_io_dispatch_overlap(request, sizeof(*request), request->rx,
                               rx_bytes))
        return NX_ERROR_INVALID;
    const nx_stm32f1_spi_dma_config_t* config = endpoint->port->config;
    const nx_result_t tx =
        nx_dma_buffer_validate(config->regions, config->region_count,
                               request->tx, tx_bytes, unit, NX_DMA_TO_DEVICE);
    return tx == NX_SUCCESS
               ? nx_dma_buffer_validate(config->regions, config->region_count,
                                        request->rx, rx_bytes, unit,
                                        NX_DMA_FROM_DEVICE)
               : tx;
}

/** \brief Admit the reviewed endpoint during Platform startup without MMIO. */
nx_result_t nx_stm32f1_spi_dma_endpoint_prepare(
    const nx_stm32f1_spi_dma_endpoint_t* endpoint) {
    if (endpoint == NULL || endpoint->port == NULL || endpoint->mode != 0U ||
        endpoint->lsb_first || endpoint->max_hz != 1000000U)
        return NX_ERROR_INVALID;
    const nx_stm32f1_spi_dma_t* state = endpoint->port;
    if (!state->ready || state->closing || state->spi.fault)
        return NX_ERROR_STATE;
    if (endpoint->cs_gpio != state->config->spi.gpio ||
        endpoint->cs_pin != state->config->spi.cs)
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}

static nx_result_t start(nx_stm32f1_spi_dma_endpoint_t* endpoint,
                         nx_spi_request_t* request, bool admitted,
                         uint8_t word_bits, uint8_t memory_flags,
                         bool retained_cs) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    const nx_result_t bound = nx_stm32f1_spi_dma_endpoint_prepare(endpoint);
    if (bound != NX_SUCCESS)
        return bound;
    nx_stm32f1_spi_dma_t* state = endpoint->port;
    const nx_result_t valid =
        validate(endpoint, request, word_bits, memory_flags);
    if (valid != NX_SUCCESS)
        return valid;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    const nx_stm32f1_spi_dma_config_t* config = state->config;
    SPI_TypeDef* spi = config->spi.registers;
    const nx_time_us_t now = nx_time_now_us();
    nx_result_t result = NX_ERROR_BUSY;
    if (state->active == NULL && source_clocks() &&
        ((config->rx->CCR | config->tx->CCR) & DMA_ENABLE) == 0U &&
        (config->dma->ISR & (RX_EVENTS | TX_EVENTS)) == 0U && spi->CR2 == 0U &&
        (spi->CR1 & SPI_CR1_SPE) == 0U &&
        (NX_STM32F1_SPI_STATUS(spi) & (SPI_IDLE | SPI_REST)) == SPI_SR_TXE &&
        vector_idle(RX_IRQ) && vector_idle(TX_IRQ) && vector_idle(SPI2_IRQn)) {
        result = nx_deadline_expired(request->base.deadline, now)
                     ? NX_ERROR_TIMEOUT
                 : admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            state->active = request;
            state->spi.active = true;
            state->draining = false;
            if (!arm(state, word_bits, memory_flags, retained_cs))
                drain(state, NX_ERROR_IO, now);
        }
    }
    nx_arch_irq_restore(guard);
    return result;
}

static nx_result_t submit(void* context, nx_spi_request_t* request) {
    return start(context, request, false, 8U, 0U, false);
}

static nx_result_t start_admitted(void* context, nx_spi_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    return start(context, request, true, 8U, 0U, false);
}

static nx_result_t cancel(void* context, nx_spi_request_t* request) {
    nx_stm32f1_spi_dma_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (request == NULL)
        return NX_ERROR_INVALID;
    if (state->active != request)
        return nx_request_state(&request->base) == NX_REQUEST_SETTLED
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}

static bool restore_lane(DMA_Channel_TypeDef* lane);

/** \brief Retire both engines and vectors before accepting an idle snapshot. */
static bool settle(nx_stm32f1_spi_dma_t* state, bool retain_cs) {
    const nx_stm32f1_spi_dma_config_t* config = state->config;
    if (!source_clocks() || !detach(config) || NX_STM32F1_IRQ_ACTIVE(RX_IRQ) ||
        NX_STM32F1_IRQ_ACTIVE(TX_IRQ) || !vector_idle(SPI2_IRQn))
        return false;
    const uint32_t flags = config->dma->ISR;
    SPI_TypeDef* spi = config->spi.registers;
    const uint32_t status = NX_STM32F1_SPI_STATUS(spi);
    if (state->terminal == NX_SUCCESS &&
        ((flags & DMA_ERRORS) != 0U || (status & SPI_REST) != 0U ||
         config->rx->CNDTR != 0U || config->tx->CNDTR != 0U))
        state->terminal = NX_ERROR_IO;
    if ((status & SPI_IDLE) != SPI_SR_TXE)
        return false;

    /* RM0008 OVR clears via DR followed by SR. The bus no longer moves. */
    if ((status & (SPI_SR_OVR | SPI_SR_RXNE)) != 0U) {
#if defined(NX_STM32F1_SPI_DMA_SEQUENCE)
        const uint8_t bits = (spi->CR1 & SPI_CR1_DFF) != 0U ? 16U : 8U;
#else
        const uint8_t bits = 8U;
#endif
        (void)NX_STM32F1_SPI_RECEIVE(spi, bits);
        (void)NX_STM32F1_SPI_STATUS(spi);
    }
    if (!nx_stm32f1_write_masked(&spi->CR1, SPI_CR1_SPE, 0U) ||
        (NX_STM32F1_SPI_STATUS(spi) & SPI_REST) != 0U)
        return false;
    /* ES096 2.4.1 allows exact TC/HT/TE bits, never CGIF for a live lane. */
    NX_STM32F1_WRITE(&config->dma->IFCR, DMA_ACK);
    NX_STM32F1_IRQ_CLEAR(RX_IRQ);
    NX_STM32F1_IRQ_CLEAR(TX_IRQ);
    nx_arch_dsb();
    if ((config->dma->ISR & (RX_EVENTS | TX_EVENTS)) != 0U ||
        !vector_idle(RX_IRQ) || !vector_idle(TX_IRQ))
        return false;
    if (retain_cs && (!restore_lane(config->rx) || !restore_lane(config->tx)))
        return false;
    if (!retain_cs && !nx_stm32f1_spi_select_cs(&state->spi, false)) {
        if (state->terminal == NX_SUCCESS)
            state->terminal = NX_ERROR_IO;
        return false;
    }

    nx_spi_request_t* request = state->active;
    const size_t count = state->terminal == NX_SUCCESS ? request->length : 0U;
    state->active = NULL;
    state->spi.active = false;
    state->spi.fault = state->spi.fault || state->terminal == NX_ERROR_IO;
    nx_request_settle(&request->base, state->terminal, count);
    return true;
}

static void service_phase(void* context, bool retain_cs) {
    nx_stm32f1_spi_dma_t* state = context;
    if (!nx_stm32f1_cold_context() || state->active == NULL)
        return;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    const nx_time_us_t now = nx_time_now_us();
    observe(state, now);
    if (state->draining && !settle(state, retain_cs) &&
        nx_deadline_expired(state->drain_deadline, now))
        (void)nx_request_transition(&state->active->base,
                                    NX_REQUEST_QUARANTINED);
    nx_arch_irq_restore(guard);
}

static void service(void* context) {
    service_phase(context, false);
}

static nx_result_t stop(void* context) {
    nx_stm32f1_spi_dma_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    state->closing = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL)
            return NX_ERROR_BUSY;
    }
    if (!state->vectors_owned)
        return NX_SUCCESS;
    NX_STM32F1_IRQ_DISABLE(RX_IRQ);
    NX_STM32F1_IRQ_DISABLE(TX_IRQ);
    NX_STM32F1_IRQ_SET_PRIORITY(RX_IRQ, state->rx_priority);
    NX_STM32F1_IRQ_SET_PRIORITY(TX_IRQ, state->tx_priority);
    nx_arch_dsb();
    if (!vector_idle(RX_IRQ) || !vector_idle(TX_IRQ) ||
        !vector_idle(SPI2_IRQn) ||
        NX_STM32F1_IRQ_PRIORITY(RX_IRQ) != state->rx_priority ||
        NX_STM32F1_IRQ_PRIORITY(TX_IRQ) != state->tx_priority)
        return NX_ERROR_IO;
    state->vectors_owned = false;
    return NX_SUCCESS;
}

static bool restore_lane(DMA_Channel_TypeDef* lane) {
    return (lane->CCR & DMA_ENABLE) == 0U &&
           nx_stm32f1_write_masked(&lane->CCR, UINT16_MAX, 0U) &&
           nx_stm32f1_write_masked(&lane->CPAR, UINT32_MAX, 0U) &&
           nx_stm32f1_write_masked(&lane->CMAR, UINT32_MAX, 0U) &&
           nx_stm32f1_write_masked(&lane->CNDTR, UINT16_MAX, 0U);
}

nx_result_t nx_stm32f1_spi_dma_close(nx_stm32f1_spi_dma_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    const nx_result_t stopped = stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL)
        return stopped;
    if (state->lanes_owned &&
        (!dma_clock() || !restore_lane(state->config->rx) ||
         !restore_lane(state->config->tx)))
        return NX_ERROR_IO;
    state->lanes_owned = false;
    const nx_result_t closed = nx_stm32f1_spi_close(&state->spi);
    if (closed != NX_SUCCESS)
        return closed;
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN, 0U))
        return NX_ERROR_IO;
    *state = (nx_stm32f1_spi_dma_t){0};
    return NX_SUCCESS;
}

static nx_result_t recover(void* context) {
    nx_stm32f1_spi_dma_t* state = context;
    return nx_stm32f1_spi_ops.recover(&state->spi);
}

static bool on_port(const void* context, const nx_spi_port_t* port) {
    const nx_stm32f1_spi_dma_endpoint_t* endpoint = context;
    return endpoint->port == port->context &&
           port->ops == &nx_stm32f1_spi_dma_ops;
}

const nx_spi_ops_t nx_stm32f1_spi_dma_ops = {
    .recover = recover, .cancel = cancel, .service = service, .stop = stop};

const nx_spi_endpoint_ops_t nx_stm32f1_spi_dma_endpoint_ops = {
    .submit = submit, .start_admitted = start_admitted, .on_port = on_port};

#if defined(NX_STM32F1_SPI_DMA_SEQUENCE)
/** \brief Check the complete outer borrow and all future data before CS. */
static nx_result_t
sequence_validate(void* context, void* child,
                  const nx_spi_dma_transaction_request_t* request) {
    nx_stm32f1_spi_dma_t* state = context;
    const nx_stm32f1_spi_dma_endpoint_t* endpoint = child;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (endpoint == NULL || endpoint->port != state)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32f1_spi_dma_endpoint_prepare(endpoint);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32f1_spi_dma_config_t* config = state->config;
    result =
        nx_spi_dma_sequence_storage_validate(request, state, sizeof(*state));
    if (result == NX_SUCCESS)
        result = nx_spi_dma_sequence_storage_validate(request, endpoint,
                                                      sizeof(*endpoint));
    if (result == NX_SUCCESS)
        result = nx_spi_dma_sequence_storage_validate(request, config,
                                                      sizeof(*config));
    if (result == NX_SUCCESS)
        result = nx_spi_dma_sequence_storage_validate(
            request, config->regions,
            config->region_count * sizeof(*config->regions));
    for (size_t i = 0U; result == NX_SUCCESS && i < request->count; ++i) {
        const nx_spi_dma_segment_t* segment = &request->segments[i];
        const nx_spi_request_t phase = {
            .base = {.deadline = request->base.deadline},
            .tx = segment->tx,
            .rx = segment->rx,
            .length = segment->words};
        result = validate(endpoint, &phase, request->word_bits,
                          segment->memory_flags);
    }
    return result;
}

/** \brief Start one phase while the outer sequence retains its exact CS. */
static nx_result_t sequence_begin(void* context, void* child,
                                  nx_spi_request_t* phase, uint8_t word_bits,
                                  uint8_t memory_flags, bool retained_cs) {
    nx_stm32f1_spi_dma_t* state = context;
    nx_stm32f1_spi_dma_endpoint_t* endpoint = child;
    if (state == NULL || endpoint == NULL || endpoint->port != state)
        return NX_ERROR_INVALID;
    if (state->spi.selected != retained_cs)
        return NX_ERROR_BUSY;
    return start(endpoint, phase, false, word_bits, memory_flags, retained_cs);
}

static void sequence_advance(void* context) {
    service_phase(context, true);
}

static nx_result_t sequence_release(void* context, void* child) {
    nx_stm32f1_spi_dma_t* state = context;
    const nx_stm32f1_spi_dma_endpoint_t* endpoint = child;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || endpoint == NULL || endpoint->port != state ||
        !state->ready)
        return NX_ERROR_INVALID;
    if (state->active != NULL || !source_clocks() ||
        !lane_clear(state->config->rx) || !lane_clear(state->config->tx) ||
        !vector_idle(RX_IRQ) || !vector_idle(TX_IRQ) || !vector_idle(SPI2_IRQn))
        return NX_ERROR_BUSY;
    SPI_TypeDef* spi = state->config->spi.registers;
    if ((spi->CR2 & SPI_REQUESTS) != 0U || (spi->CR1 & SPI_CR1_SPE) != 0U ||
        (NX_STM32F1_SPI_STATUS(spi) & (SPI_IDLE | SPI_REST)) != SPI_SR_TXE)
        return NX_ERROR_BUSY;
    return nx_stm32f1_spi_select_cs(&state->spi, false) ? NX_SUCCESS
                                                        : NX_ERROR_IO;
}

/** \brief Cold unwind can follow an earlier prepare rejection. */
static nx_result_t sequence_stop(void* context) {
    nx_stm32f1_spi_dma_t* state = context;
    return state->config == NULL ? NX_SUCCESS : stop(state);
}

const nx_spi_dma_sequence_backend_t nx_stm32f1_spi_dma_sequence_backend = {
    .validate = sequence_validate,
    .begin = sequence_begin,
    .advance = sequence_advance,
    .cancel = cancel,
    .release = sequence_release,
    .stop = sequence_stop,
};

bool nx_stm32f1_spi_dma_owner_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port) {
    if (!nx_spi_dma_sequence_contains(endpoint, port))
        return false;
    const nx_spi_dma_sequence_state_t* sequence = port->context;
    const nx_spi_dma_sequence_endpoint_t* child = endpoint->context;
    const nx_stm32f1_spi_dma_endpoint_t* device = child->endpoint;
    return sequence->backend == &nx_stm32f1_spi_dma_sequence_backend &&
           sequence->context != NULL && device->port == sequence->context;
}

nx_result_t
nx_stm32f1_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t syscall_ceiling) {
    (void)port;
    (void)wake;
    (void)syscall_ceiling;
    return NX_ERROR_UNSUPPORTED;
}

nx_result_t
nx_stm32f1_spi_dma_sequence_close(nx_spi_dma_sequence_state_t* sequence,
                                  nx_stm32f1_spi_dma_t* owner) {
    const nx_spi_dma_transaction_port_t port = {&nx_spi_dma_sequence_port_ops,
                                                sequence};
    const nx_result_t stopped = nx_spi_dma_transaction_stop(&port);
    if (stopped != NX_SUCCESS || owner->config == NULL)
        return stopped;
    return nx_stm32f1_spi_dma_close(owner);
}
#endif
