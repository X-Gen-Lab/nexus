/**
 * \file            spi_dma.c
 *
 * \brief           Static full-duplex SPI DMA with independent CS and memory
 *                  drain
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_dma.h"
#include "stm32f4xx_ll_spi.h"

#ifndef NX_STM32_DMA_DISABLE
#define NX_STM32_DMA_DISABLE(stream, mask) ((stream)->CR &= ~(uint32_t)(mask))
#endif

#ifndef NX_STM32_SPI_DMA_ACK
#define NX_STM32_SPI_DMA_ACK(clear, mask) (*(clear) = (uint32_t)(mask))
#endif

/** \brief Selected register identity determines the immutable reviewed route.
 */
static bool primary(const nx_stm32_spi_dma_state_t* state) {
    return state->spi.registers != SPI2;
}
/** \brief Each selected engine's vector is independent of DMA request
 * selectors. */
static IRQn_Type dma_irq(const nx_stm32_spi_dma_state_t* state, bool receive) {
    if (primary(state))
        return receive ? DMA2_Stream0_IRQn : DMA2_Stream3_IRQn;
    return receive ? DMA1_Stream3_IRQn : DMA1_Stream4_IRQn;
}
/** \brief The second pair straddles low and high status banks. */
static uint32_t flag_shift(const nx_stm32_spi_dma_state_t* state,
                           bool receive) {
    if (primary(state))
        return receive ? 0U : 22U;
    return receive ? 22U : 0U;
}
/** \brief Observe only the exact engine's retained terminal group. */
static uint32_t flags(const nx_stm32_spi_dma_state_t* state, bool receive) {
    uint32_t bank =
        primary(state) || receive ? state->dma->LISR : state->dma->HISR;
    return (bank >> flag_shift(state, receive)) & 0x3DU;
}

/** \brief W1C only the maintained SPI streams and preserve UART/ADC neighbours.
 */
static void acknowledge_bits(nx_stm32_spi_dma_state_t* state, bool receive,
                             uint32_t observed) {
    volatile uint32_t* clear =
        primary(state) || receive ? &state->dma->LIFCR : &state->dma->HIFCR;
    NX_STM32_SPI_DMA_ACK(clear, observed << flag_shift(state, receive));
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(dma_irq(state, receive));
#endif
}

/** \brief Cold/reset cleanup owns the selected stale flag group. */
static void acknowledge(nx_stm32_spi_dma_state_t* state, bool receive) {
    acknowledge_bits(state, receive, 0x3DU);
}

/** \brief Withdraw DMA requests before disabling their memory engines. */
static void detach(nx_stm32_spi_dma_state_t* state, bool receive) {
    DMA_Stream_TypeDef* stream = receive ? state->rx : state->tx;
    if (receive) {
        LL_SPI_DisableDMAReq_RX(state->spi.registers);
    } else {
        LL_SPI_DisableDMAReq_TX(state->spi.registers);
    }
    NX_STM32_DMA_DISABLE(stream, DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                     DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
    nx_arch_dsb();
    /* Preserve errors first observed after the IRQ terminal snapshot. */
    acknowledge_bits(state, receive, flags(state, receive) & 0x30U);
}

/** \brief Refuse alternate controller routes before acquiring any request. */
nx_result_t nx_stm32_spi_dma_initialize(nx_stm32_spi_dma_state_t* state) {
    if (state == NULL || state->spi.registers == NULL ||
        state->spi.rcc == NULL || state->spi.clock_hz == 0U ||
        state->dma == NULL || state->tx == NULL || state->rx == NULL ||
        state->tx == state->rx || state->regions == NULL ||
        state->region_count == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
#ifndef NEXUS_STM32_MODEL
    bool first = state->spi.registers == SPI1;
    if ((!first && state->spi.registers != SPI2) || state->spi.rcc != RCC ||
        state->spi.clock_hz != (first ? 84000000U : 42000000U) ||
        state->dma != (first ? DMA2 : DMA1) ||
        state->tx != (first ? DMA2_Stream3 : DMA1_Stream4) ||
        state->rx != (first ? DMA2_Stream0 : DMA1_Stream3)) {
        return NX_ERROR_UNSUPPORTED;
    }
#endif
    if (state->initialized || state->spi.active || state->active != NULL ||
        ((state->tx->CR | state->rx->CR) & DMA_SxCR_EN) != 0U ||
        (state->spi.registers->SR & SPI_SR_BSY) != 0U) {
        return NX_ERROR_BUSY;
    }
#ifndef NEXUS_STM32_MODEL
    /* An idle peripheral still has an owner while its vectors are retained. */
    unsigned rx_vector = (unsigned)dma_irq(state, true);
    unsigned tx_vector = (unsigned)dma_irq(state, false);
    if ((NVIC->ISER[rx_vector / 32U] & (1UL << (rx_vector % 32U))) != 0U ||
        (NVIC->ISER[tx_vector / 32U] & (1UL << (tx_vector % 32U))) != 0U) {
        return NX_ERROR_BUSY;
    }
#endif
    state->initialized = true;
    state->stopping = false;
    state->draining = false;
    state->spi.fault = false;
    acknowledge(state, false);
    acknowledge(state, true);
#ifndef NEXUS_STM32_MODEL
    NVIC_SetPriority(dma_irq(state, true), 5U);
    NVIC_SetPriority(dma_irq(state, false), 5U);
    NVIC_EnableIRQ(dma_irq(state, true));
    NVIC_EnableIRQ(dma_irq(state, false));
#endif
    return NX_SUCCESS;
}

/** \brief Validate CS, exact bus identity and frequency before hardware
 * effects. */
static nx_result_t configuration(nx_stm32_spi_dma_endpoint_state_t* device,
                                 uint32_t* control) {
    if (device == NULL || device->dma == NULL || control == NULL ||
        device->endpoint.port != &device->dma->spi) {
        return NX_ERROR_INVALID;
    }
    const nx_stm32_spi_endpoint_state_t* endpoint = &device->endpoint;
    const nx_stm32_gpio_state_t* cs = endpoint->cs;
    if (cs == NULL || cs->registers == NULL || !cs->initialized ||
        !cs->output || endpoint->cs_mask == 0U ||
        (endpoint->cs_mask & ~cs->mask) != 0U || endpoint->mode > 3U ||
        endpoint->frequency_hz == 0U) {
        return NX_ERROR_INVALID;
    }
    uint32_t divider = 2U;
    uint32_t baud_bits = 0U;
    while (device->dma->spi.clock_hz / divider > endpoint->frequency_hz &&
           divider < 256U) {
        divider *= 2U;
        ++baud_bits;
    }
    if (device->dma->spi.clock_hz / divider > endpoint->frequency_hz) {
        return NX_ERROR_UNSUPPORTED;
    }
    *control = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (baud_bits << 3U) |
               ((endpoint->mode & 1U) != 0U ? SPI_CR1_CPHA : 0U) |
               ((endpoint->mode & 2U) != 0U ? SPI_CR1_CPOL : 0U);
    return NX_SUCCESS;
}

/** \brief The full-duplex mode never allocates scratch buffers or copies
 * payload. */
static nx_result_t validate_buffers(nx_stm32_spi_dma_state_t* state,
                                    const nx_spi_request_t* request,
                                    uint8_t memory_flags) {
    if ((memory_flags & ~(NX_SPI_MEMORY_TX_FIXED | NX_SPI_MEMORY_RX_FIXED)) !=
        0U) {
        return NX_ERROR_INVALID;
    }
    if (request == NULL || request->length == 0U || request->length > 65535U) {
        return NX_ERROR_INVALID;
    }
    if (request->tx == NULL || request->rx == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t tx_span =
        (memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U ? 1U : request->length;
    size_t rx_span =
        (memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U ? 1U : request->length;
    nx_result_t result =
        nx_dma_buffer_validate(state->regions, state->region_count, request->tx,
                               tx_span, 1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    result =
        nx_dma_buffer_validate(state->regions, state->region_count, request->rx,
                               rx_span, 1U, NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    uintptr_t tx = (uintptr_t)request->tx;
    uintptr_t rx = (uintptr_t)request->rx;
    return tx < rx + rx_span && rx < tx + tx_span ? NX_ERROR_INVALID
                                                  : NX_SUCCESS;
}

/** \brief Program exactly one disabled DMA memory engine. */
static void program(nx_stm32_spi_dma_state_t* state, bool receive,
                    nx_spi_request_t* request) {
    DMA_Stream_TypeDef* stream = receive ? state->rx : state->tx;
    acknowledge(state, receive);
    stream->PAR = (uint32_t)(uintptr_t)&state->spi.registers->DR;
    stream->M0AR = (uint32_t)(uintptr_t)(receive ? request->rx : request->tx);
    stream->NDTR = (uint32_t)request->length;
    stream->FCR = 0U;
    uint8_t fixed = receive ? NX_SPI_MEMORY_RX_FIXED : NX_SPI_MEMORY_TX_FIXED;
    uint32_t increment =
        (state->memory_flags & fixed) != 0U ? 0U : DMA_SxCR_MINC;
    stream->CR = ((primary(state) ? 3U : 0U) << DMA_SxCR_CHSEL_Pos) |
                 increment | DMA_SxCR_PL_1 | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                 DMA_SxCR_DMEIE | (receive ? 0U : DMA_SxCR_DIR_0);
}

/** \brief One admission gates both DMA engines and the entire selected CS
 * interval. */
static nx_result_t start(nx_stm32_spi_dma_endpoint_state_t* device,
                         nx_spi_request_t* request, bool admitted,
                         uint8_t memory_flags) {
    uint32_t control;
    nx_result_t result = configuration(device, &control);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_stm32_spi_dma_state_t* state = device->dma;
    result = validate_buffers(state, request, memory_flags);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->initialized || state->stopping || state->spi.fault) {
        result = NX_ERROR_STATE;
    } else if (state->active != NULL || state->spi.active ||
               ((state->tx->CR | state->rx->CR) & DMA_SxCR_EN) != 0U ||
               (state->spi.registers->SR & SPI_SR_BSY) != 0U) {
        result = NX_ERROR_BUSY;
    } else if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        result = NX_ERROR_TIMEOUT;
    } else {
        result = admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    }
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    state->memory_flags = memory_flags;
    state->active = request;
    state->endpoint = device;
    state->spi.active = true;
    state->terminal = NX_SUCCESS;
    state->draining = false;
    state->tx_complete = false;
    state->rx_complete = false;
    state->drain_deadline = NX_DEADLINE_NEVER;
    state->spi.registers->CR1 = control;
    state->spi.registers->CR2 = 0U;
    program(state, true, request);
    program(state, false, request);
    (void)nx_stm32_gpio_write(device->endpoint.cs, 0U,
                              device->endpoint.cs_mask);
    nx_arch_dsb();
    state->rx->CR |= DMA_SxCR_EN;
    state->tx->CR |= DMA_SxCR_EN;
    state->spi.registers->CR2 = SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;
    LL_SPI_Enable(state->spi.registers);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Start direct finite DMA after every rejection condition is resolved.
 */
static nx_result_t submit(void* context, nx_spi_request_t* request) {
    return start(context, request, false, 0U);
}

/** \brief Copy explicit memory modes only after valid request admission. */
static nx_result_t submit_memory(void* context, nx_spi_request_t* request,
                                 uint8_t memory_flags) {
    return start(context, request, false, memory_flags);
}

/** \brief Transfer an existing adapter loan to the exact selected endpoint. */
static nx_result_t start_admitted(void* context, nx_spi_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(context, request, true, 0U);
}

/** \brief Stopping DMA does not withdraw CS before an actual SPI idle
 * observation. */
static void begin_drain(nx_stm32_spi_dma_state_t* state, nx_result_t terminal) {
    detach(state, false);
    detach(state, true);
    state->draining = true;
    state->terminal = terminal;
    state->drain_deadline = nx_deadline_after(nx_time_now_us(), 200U);
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
}

/** \brief Cancellation never reclaims memory or derives wire count from NDTR.
 */
static nx_result_t cancel(void* context, nx_spi_request_t* request) {
    nx_stm32_spi_dma_state_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(saved);
        return result;
    }
    if (!state->draining) {
        begin_drain(state, NX_ERROR_CANCELLED);
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Each DMA source latches its own completion before the task proves
 * wire idle. */
void nx_stm32_spi_dma_irq(nx_stm32_spi_dma_state_t* state, bool receive) {
    if (state == NULL || !state->initialized) {
        return;
    }
    uint32_t status = flags(state, receive);
    if ((status & 0x2DU) == 0U) {
        return;
    }
    if (state->active == NULL || state->draining) {
        if (state->active == NULL) {
            acknowledge(state, receive);
        } else {
            acknowledge_bits(state, receive, status & 0x30U);
        }
        return;
    }
    DMA_Stream_TypeDef* stream = receive ? state->rx : state->tx;
    if ((status & 0x0DU) != 0U || stream->NDTR != 0U) {
        begin_drain(state, NX_ERROR_IO);
    } else {
        detach(state, receive);
        if ((flags(state, receive) & 0x0DU) != 0U) {
            begin_drain(state, NX_ERROR_IO);
        } else if (receive) {
            state->rx_complete = true;
        } else {
            state->tx_complete = true;
        }
        if (state->tx_complete && state->rx_complete) {
            state->drain_deadline = nx_deadline_after(nx_time_now_us(), 200U);
        }
    }
    (void)nx_irq_wake_signal(state->wake);
}

/** \brief SETTLED follows two engine proofs and true peripheral idle, then CS
 * release. */
static void service(void* context) {
    nx_stm32_spi_dma_state_t* state = context;
    if (state == NULL || nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_spi_request_t* request = state->active;
    if (request == NULL) {
        nx_arch_irq_restore(saved);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    if (!state->draining &&
        ((flags(state, true) | flags(state, false)) & 0x0DU) != 0U) {
        begin_drain(state, NX_ERROR_IO);
    }
    uint32_t status = state->spi.registers->SR;
    if (!state->draining && (status & (SPI_SR_OVR | SPI_SR_MODF)) != 0U) {
        begin_drain(state, NX_ERROR_IO);
    } else if (!state->draining &&
               nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT);
    }
    bool idle = (status & SPI_SR_BSY) == 0U && (status & SPI_SR_TXE) != 0U;
    bool terminal =
        state->draining || (state->tx_complete && state->rx_complete);
    uint32_t irq_mask =
        DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    nx_dma_drain_facts_t tx = {
        .length = request->length,
        .remaining = state->tx->NDTR,
        .engine_disabled = (state->tx->CR & DMA_SxCR_EN) == 0U,
        .irq_detached = (state->tx->CR & irq_mask) == 0U,
        .peripheral_idle = idle,
    };
    nx_dma_drain_facts_t rx = tx;
    rx.remaining = state->rx->NDTR;
    rx.engine_disabled = (state->rx->CR & DMA_SxCR_EN) == 0U;
    rx.irq_detached = (state->rx->CR & irq_mask) == 0U;
    if (terminal && nx_dma_drain_check(&tx) == NX_SUCCESS &&
        nx_dma_drain_check(&rx) == NX_SUCCESS) {
        state->spi.registers->CR2 = 0U;
        LL_SPI_Disable(state->spi.registers);
        (void)state->spi.registers->DR;
        (void)state->spi.registers->SR;
        nx_arch_dsb();
        nx_stm32_spi_endpoint_state_t* endpoint = &state->endpoint->endpoint;
        (void)nx_stm32_gpio_write(endpoint->cs, endpoint->cs_mask, 0U);
        /* Both engines are actually disabled; do not erase a late error. */
        if (state->terminal == NX_SUCCESS &&
            ((flags(state, true) | flags(state, false)) & 0x0DU) != 0U) {
            state->terminal = NX_ERROR_IO;
        }
        nx_result_t result = state->terminal;
        size_t count = result == NX_SUCCESS ? request->length : 0U;
        state->spi.fault = result == NX_ERROR_IO;
        state->active = NULL;
        state->endpoint = NULL;
        state->spi.active = false;
        nx_request_settle(&request->base, result, count);
    } else {
        if (!state->draining && state->tx_complete && state->rx_complete &&
            nx_deadline_expired(state->drain_deadline, now)) {
            nx_time_us_t deadline = state->drain_deadline;
            begin_drain(state, NX_ERROR_IO);
            state->drain_deadline = deadline;
        }
        if (state->draining &&
            nx_deadline_expired(state->drain_deadline, now) &&
            nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
    nx_arch_irq_restore(saved);
}

/** \brief One stopped controller retains no DMA, IRQ or selected endpoint
 * access. */
static nx_result_t stop(void* context) {
    nx_stm32_spi_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    state->stopping = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    detach(state, false);
    detach(state, true);
    if (((state->tx->CR | state->rx->CR) & DMA_SxCR_EN) != 0U ||
        (state->spi.registers->SR & SPI_SR_BSY) != 0U) {
        return NX_ERROR_BUSY;
    }
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(dma_irq(state, true));
    NVIC_DisableIRQ(dma_irq(state, false));
    NVIC_ClearPendingIRQ(dma_irq(state, true));
    NVIC_ClearPendingIRQ(dma_irq(state, false));
#endif
    state->wake = NULL;
    state->initialized = false;
    state->spi.registers->CR2 = 0U;
    LL_SPI_Disable(state->spi.registers);
    return NX_SUCCESS;
}

/** \brief A recovered idle bus never replays the failed request. */
static nx_result_t recover(void* context) {
    nx_stm32_spi_dma_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID : nx_stm32_spi_recover(&state->spi);
}

/** \brief Both TX and RX publishers must be legal for a kernel-calling sink. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32_spi_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_stm32_irq_wake_validate(wake, dma_irq(state, false),
                                                    syscall_ceiling);
    if (result == NX_SUCCESS) {
        result = nx_stm32_irq_wake_validate(wake, dma_irq(state, true),
                                            syscall_ceiling);
    }
    if (result == NX_SUCCESS) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        state->wake = wake;
        nx_arch_irq_restore(saved);
    }
    return result;
}

/** \brief Optional polling keeps the same arbitration gate as finite DMA. */
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    nx_stm32_spi_dma_endpoint_state_t* device = context;
    if (device == NULL || device->dma == NULL || !device->dma->initialized ||
        device->dma->stopping) {
        return NX_ERROR_STATE;
    }
    return nx_stm32_spi_endpoint_transfer(&device->endpoint, tx, rx, length,
                                          deadline, transferred);
}

/** \brief Match exact bus state and mode before adapter admission. */
static bool on_port(const void* context, const nx_spi_port_t* port) {
    const nx_stm32_spi_dma_endpoint_state_t* device = context;
    return device != NULL && port != NULL && device->dma == port->context &&
           port->ops == &nx_stm32_spi_dma_ops;
}

/** \brief One immutable controller table per maintained finite DMA mode. */
const nx_spi_ops_t nx_stm32_spi_dma_ops = {
    .recover = recover,
    .cancel = cancel,
    .service = service,
    .stop = stop,
    .attach_wake = attach_wake,
};

/** \brief Independent CS endpoints share methods and exact controller
 * arbitration. */
const nx_spi_endpoint_ops_t nx_stm32_spi_dma_endpoint_ops = {
    .transfer = transfer,
    .submit = submit,
    .submit_memory = submit_memory,
    .start_admitted = start_admitted,
    .on_port = on_port,
};
