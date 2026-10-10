/**
 * \file            spi_dma.c
 * \brief           Fixed SPI4 finite DMA with independent memory and wire drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_spi_dma.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "private/system.h"

#ifndef NX_GD32_SPI_DMA_DISABLE
#define NX_GD32_SPI_DMA_DISABLE(receive, mask)                                 \
    do {                                                                       \
        if (receive) {                                                         \
            DMA_CH3CTL(DMA1) &= ~(uint32_t)(mask);                             \
        } else {                                                               \
            DMA_CH4CTL(DMA1) &= ~(uint32_t)(mask);                             \
        }                                                                      \
    } while (0)
#endif
#ifndef NX_GD32_SPI_DMA_ACK
#define NX_GD32_SPI_DMA_ACK(receive, mask)                                     \
    do {                                                                       \
        if (receive) {                                                         \
            DMA_INTC0(DMA1) = (mask);                                          \
        } else {                                                               \
            DMA_INTC1(DMA1) = (mask);                                          \
        }                                                                      \
    } while (0)
#endif

/** \brief Exact channel3 low and channel4 high interrupt flag placement. */
static uint32_t flags(bool receive) {
    return receive ? (DMA_INTF0(DMA1) >> 22U) & 0x3DU : DMA_INTF1(DMA1) & 0x3DU;
}

/** \brief W1C selected flags only; UART channel7 and other neighbours survive.
 */
static void acknowledge(bool receive) {
    NX_GD32_SPI_DMA_ACK(receive, receive ? 0x3DU << 22U : 0x3DU);
    NVIC_ClearPendingIRQ(receive ? DMA1_Channel3_IRQn : DMA1_Channel4_IRQn);
}

/** \brief Withdraw peripheral requests before disabling their memory engine. */
static void detach(bool receive) {
    SPI_CTL1(SPI4) &= ~(uint32_t)(receive ? SPI_CTL1_DMAREN : SPI_CTL1_DMATEN);
    NX_GD32_SPI_DMA_DISABLE(receive, DMA_CHXCTL_CHEN | DMA_CHXCTL_FTFIE |
                                         DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE |
                                         DMA_CHXCTL_SDEIE);
    nx_gd32_peripheral_barrier();
    acknowledge(receive);
}

/** \brief Acquire only fixed SPI4 and its two reviewed DMA publishers. */
nx_result_t nx_gd32_spi_dma_initialize(
    nx_gd32_spi_dma_state_t* state, nx_gd32_spi_dma_endpoint_state_t* endpoint,
    uint32_t cs_gpio, uint32_t cs_mask, uint32_t maximum_hz, unsigned mode,
    unsigned priority) {
    if (state == NULL || endpoint == NULL || state->regions == NULL ||
        state->region_count == 0U || priority > 15U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->initialized || state->spi.initialized || state->active != NULL ||
        ((DMA_CH3CTL(DMA1) | DMA_CH4CTL(DMA1)) & DMA_CHXCTL_CHEN) != 0U ||
        (SPI_STAT(SPI4) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_gd32_spi_initialize_at(
        &state->spi, &endpoint->endpoint, &nx_gd32_spi4_controller, cs_gpio,
        cs_mask, maximum_hz, mode);
    if (result != NX_SUCCESS) {
        return result;
    }
    endpoint->dma = state;
    state->stopping = false;
    state->draining = false;
    state->faulted = false;
    state->tx_complete = false;
    state->rx_complete = false;
    state->endpoint = NULL;
    state->wake = NULL;
    state->initialized = true;
    acknowledge(true);
    acknowledge(false);
    NVIC_SetPriority(DMA1_Channel3_IRQn, priority);
    NVIC_SetPriority(DMA1_Channel4_IRQn, priority);
    NVIC_EnableIRQ(DMA1_Channel3_IRQn);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    return NX_SUCCESS;
}

/** \brief Validate immutable child identity and CS without touching hardware.
 */
static nx_result_t
configuration(const nx_gd32_spi_dma_endpoint_state_t* device) {
    if (device == NULL || device->dma == NULL ||
        device->endpoint.port != &device->dma->spi ||
        device->dma->spi.controller != &nx_gd32_spi4_controller ||
        device->endpoint.cs_gpio < GPIOA || device->endpoint.cs_gpio > GPIOI ||
        (device->endpoint.cs_gpio - GPIOA) % (GPIOB - GPIOA) != 0U ||
        device->endpoint.cs_mask == 0U ||
        (device->endpoint.cs_mask & ~UINT32_C(0xFFFF)) != 0U) {
        return NX_ERROR_INVALID;
    }
    const uint32_t required =
        SPI_CTL0_MSTMOD | SPI_CTL0_SWNSSEN | SPI_CTL0_SWNSS;
    const uint32_t allowed =
        required | SPI_CTL0_PSC | SPI_CTL0_CKPH | SPI_CTL0_CKPL;
    return (device->endpoint.control & required) == required &&
                   (device->endpoint.control & ~allowed) == 0U
               ? NX_SUCCESS
               : NX_ERROR_INVALID;
}

/** \brief Full duplex borrows two disjoint domains; no copy or scratch buffer.
 */
static nx_result_t validate_buffers(const nx_gd32_spi_dma_state_t* state,
                                    const nx_spi_request_t* request) {
    if (request == NULL || request->length == 0U || request->length > 65535U) {
        return NX_ERROR_INVALID;
    }
    if (request->tx == NULL || request->rx == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result =
        nx_dma_buffer_validate(state->regions, state->region_count, request->tx,
                               request->length, 1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    result =
        nx_dma_buffer_validate(state->regions, state->region_count, request->rx,
                               request->length, 1U, NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    uintptr_t tx = (uintptr_t)request->tx;
    uintptr_t rx = (uintptr_t)request->rx;
    return tx < rx + request->length && rx < tx + request->length
               ? NX_ERROR_INVALID
               : NX_SUCCESS;
}

/** \brief Program exact disabled engines; 8-bit incrementing memory, selector2.
 */
static void program(const nx_spi_request_t* request, bool receive) {
    acknowledge(receive);
    uint32_t control = (2U << 25U) | DMA_CHXCTL_MNAGA | (1U << 17U) |
                       DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE |
                       (receive ? 0U : 1U << 6U);
    if (receive) {
        DMA_CH3PADDR(DMA1) = (uint32_t)(uintptr_t)&SPI_DATA(SPI4);
        DMA_CH3M0ADDR(DMA1) = (uint32_t)(uintptr_t)request->rx;
        DMA_CH3CNT(DMA1) = (uint32_t)request->length;
        DMA_CH3FCTL(DMA1) = 0U;
        DMA_CH3CTL(DMA1) = control;
    } else {
        DMA_CH4PADDR(DMA1) = (uint32_t)(uintptr_t)&SPI_DATA(SPI4);
        DMA_CH4M0ADDR(DMA1) = (uint32_t)(uintptr_t)request->tx;
        DMA_CH4CNT(DMA1) = (uint32_t)request->length;
        DMA_CH4FCTL(DMA1) = 0U;
        DMA_CH4CTL(DMA1) = control;
    }
}

/** \brief One successful admission gates both engines and the whole CS
 * interval. */
static nx_result_t start(nx_gd32_spi_dma_endpoint_state_t* device,
                         nx_spi_request_t* request, bool admitted) {
    nx_result_t result = configuration(device);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_gd32_spi_dma_state_t* state = device->dma;
    result = validate_buffers(state, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (!state->initialized || state->stopping || state->faulted) {
        result = NX_ERROR_STATE;
    } else if (state->active != NULL || state->spi.active ||
               ((DMA_CH3CTL(DMA1) | DMA_CH4CTL(DMA1)) & DMA_CHXCTL_CHEN) !=
                   0U ||
               (SPI_STAT(SPI4) & SPI_STAT_TRANS) != 0U) {
        result = NX_ERROR_BUSY;
    } else if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        result = NX_ERROR_TIMEOUT;
    } else {
        result = admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    }
    if (result != NX_SUCCESS) {
        nx_gd32_critical_leave(saved);
        return result;
    }
    state->active = request;
    state->endpoint = device;
    state->spi.active = true;
    state->terminal = NX_SUCCESS;
    state->draining = false;
    state->tx_complete = false;
    state->rx_complete = false;
    state->drain_deadline = NX_DEADLINE_NEVER;
    SPI_CTL0(SPI4) = device->endpoint.control;
    SPI_CTL1(SPI4) = 0U;
    program(request, true);
    program(request, false);
    GPIO_BOP(device->endpoint.cs_gpio) = device->endpoint.cs_mask << 16U;
    nx_gd32_peripheral_barrier();
    DMA_CH3CTL(DMA1) |= DMA_CHXCTL_CHEN;
    DMA_CH4CTL(DMA1) |= DMA_CHXCTL_CHEN;
    SPI_CTL1(SPI4) = SPI_CTL1_DMAREN | SPI_CTL1_DMATEN;
    SPI_CTL0(SPI4) |= SPI_CTL0_SPIEN;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief Rejectable conditions are resolved before the provider creates a
 * loan. */
static nx_result_t submit(void* context, nx_spi_request_t* request) {
    return start(context, request, false);
}

/** \brief Transfer an existing adapter loan without second admission. */
static nx_result_t start_admitted(void* context, nx_spi_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(context, request, true);
}

/** \brief Withdraw both memory sources while preserving CS and all references.
 */
static void begin_drain(nx_gd32_spi_dma_state_t* state, nx_result_t terminal) {
    detach(false);
    detach(true);
    state->draining = true;
    state->terminal = terminal;
    state->drain_deadline = nx_deadline_after(nx_time_now_us(), 200U);
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
}

/** \brief Cancel is a drain request; residual counters never become wire bytes.
 */
static nx_result_t cancel(void* context, nx_spi_request_t* request) {
    nx_gd32_spi_dma_state_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_result_t result = NX_SUCCESS;
    if (state->active != request) {
        result = nx_request_state(&request->base) == NX_REQUEST_SETTLED
                     ? NX_SUCCESS
                     : NX_ERROR_STATE;
    } else if (!state->draining) {
        begin_drain(state, NX_ERROR_CANCELLED);
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Latch each memory completion once; no IRQ accesses a settled request.
 */
void nx_gd32_spi_dma_irq(nx_gd32_spi_dma_state_t* state, bool receive) {
    if (state == NULL || !state->initialized) {
        return;
    }
    uint32_t saved = nx_gd32_critical_enter();
    uint32_t status = flags(receive);
    if ((status & 0x2DU) == 0U) {
        nx_gd32_critical_leave(saved);
        return;
    }
    if (state->active == NULL || state->draining ||
        (receive ? state->rx_complete : state->tx_complete)) {
        acknowledge(receive);
        nx_gd32_critical_leave(saved);
        return;
    }
    uint32_t remaining = receive ? DMA_CH3CNT(DMA1) : DMA_CH4CNT(DMA1);
    if ((status & 0x0DU) != 0U || remaining != 0U) {
        begin_drain(state, NX_ERROR_IO);
    } else {
        detach(receive);
        if (receive) {
            state->rx_complete = true;
        } else {
            state->tx_complete = true;
        }
        if (state->tx_complete && state->rx_complete) {
            /* An unbounded transfer deadline cannot hide a failed drain. */
            state->drain_deadline = nx_deadline_after(nx_time_now_us(), 200U);
        }
    }
    /* Task-only attachment cannot reclaim this snapshot before IRQ return.
     * Publish facts first, then permit the wake's kernel call outside PRIMASK.
     */
    const nx_irq_wake_t* wake = state->wake;
    nx_gd32_critical_leave(saved);
    (void)nx_irq_wake_signal(wake);
}

/** \brief Two disabled engines, detached IRQs and idle wire precede
 * publication. */
static void service(void* context) {
    nx_gd32_spi_dma_state_t* state = context;
    if (state == NULL || nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_spi_request_t* request = state->active;
    if (request == NULL) {
        nx_gd32_critical_leave(saved);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    uint32_t status = SPI_STAT(SPI4);
    if (!state->draining && (status & (SPI_STAT_CONFERR | SPI_STAT_RXORERR |
                                       SPI_STAT_FERR)) != 0U) {
        begin_drain(state, NX_ERROR_IO);
    } else if (!state->draining &&
               nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT);
    }
    bool idle =
        (status & SPI_STAT_TRANS) == 0U && (status & SPI_STAT_TBE) != 0U;
    bool terminal =
        state->draining || (state->tx_complete && state->rx_complete);
    uint32_t irq_mask = DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE |
                        DMA_CHXCTL_SDEIE;
    nx_dma_drain_facts_t tx = {
        .length = request->length,
        .remaining = DMA_CH4CNT(DMA1),
        .engine_disabled = (DMA_CH4CTL(DMA1) & DMA_CHXCTL_CHEN) == 0U,
        .irq_detached = (DMA_CH4CTL(DMA1) & irq_mask) == 0U,
        .peripheral_idle = idle,
    };
    nx_dma_drain_facts_t rx = tx;
    rx.remaining = DMA_CH3CNT(DMA1);
    rx.engine_disabled = (DMA_CH3CTL(DMA1) & DMA_CHXCTL_CHEN) == 0U;
    rx.irq_detached = (DMA_CH3CTL(DMA1) & irq_mask) == 0U;
    if (terminal && nx_dma_drain_check(&tx) == NX_SUCCESS &&
        nx_dma_drain_check(&rx) == NX_SUCCESS) {
        SPI_CTL1(SPI4) = 0U;
        SPI_CTL0(SPI4) &= ~(uint32_t)SPI_CTL0_SPIEN;
        (void)SPI_DATA(SPI4);
        (void)SPI_STAT(SPI4);
        nx_gd32_peripheral_barrier();
        const nx_gd32_spi_endpoint_state_t* endpoint =
            &state->endpoint->endpoint;
        GPIO_BOP(endpoint->cs_gpio) = endpoint->cs_mask;
        nx_gd32_peripheral_barrier();
        nx_result_t result = state->terminal;
        size_t count = result == NX_SUCCESS ? request->length : 0U;
        state->faulted = result == NX_ERROR_IO;
        state->active = NULL;
        state->endpoint = NULL;
        state->spi.active = false;
        nx_request_settle(&request->base, result, count);
    } else {
        if (!state->draining && terminal &&
            nx_deadline_expired(state->drain_deadline, now)) {
            nx_time_us_t expired = state->drain_deadline;
            begin_drain(state, NX_ERROR_IO);
            state->drain_deadline = expired;
        }
        if (state->draining &&
            nx_deadline_expired(state->drain_deadline, now) &&
            nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
    nx_gd32_critical_leave(saved);
}

/** \brief Close admission; retain acquired clocks/vectors until actual drain.
 */
static nx_result_t stop(void* context) {
    nx_gd32_spi_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized) {
        return NX_SUCCESS;
    }
    state->stopping = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    detach(false);
    detach(true);
    if (((DMA_CH3CTL(DMA1) | DMA_CH4CTL(DMA1)) & DMA_CHXCTL_CHEN) != 0U ||
        (SPI_STAT(SPI4) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_gd32_spi_stop(&state->spi);
    if (result != NX_SUCCESS) {
        return result;
    }
    NVIC_DisableIRQ(DMA1_Channel3_IRQn);
    NVIC_DisableIRQ(DMA1_Channel4_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel3_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
    state->wake = NULL;
    state->initialized = false;
    return NX_SUCCESS;
}

/** \brief Explicit idle recovery clears a fault without replay or clock theft.
 */
static nx_result_t recover(void* context) {
    nx_gd32_spi_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL ||
        ((DMA_CH3CTL(DMA1) | DMA_CH4CTL(DMA1)) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_gd32_spi_recover(&state->spi);
    if (result == NX_SUCCESS) {
        state->faulted = false;
    }
    return result;
}

/** \brief Validate both actual publishers before borrowing a wake target. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_gd32_spi_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_irq_wake_validate(
        wake, (uint8_t)NVIC_GetPriority(DMA1_Channel3_IRQn), 4U,
        syscall_ceiling);
    if (result == NX_SUCCESS) {
        result = nx_irq_wake_validate(
            wake, (uint8_t)NVIC_GetPriority(DMA1_Channel4_IRQn), 4U,
            syscall_ceiling);
    }
    if (result == NX_SUCCESS) {
        uint32_t saved = nx_gd32_critical_enter();
        state->wake = wake;
        nx_gd32_critical_leave(saved);
    }
    return result;
}

/** \brief Optional bounded polling shares the same controller arbitration. */
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    nx_gd32_spi_dma_endpoint_state_t* device = context;
    nx_result_t result = configuration(device);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!device->dma->initialized || device->dma->stopping ||
        device->dma->faulted) {
        return NX_ERROR_STATE;
    }
    return nx_gd32_spi_endpoint_transfer(&device->endpoint, tx, rx, length,
                                         deadline, transferred);
}

/** \brief Prove exact compatible bus ownership before an adapter borrows work.
 */
static bool on_port(const void* context, const nx_spi_port_t* port) {
    const nx_gd32_spi_dma_endpoint_state_t* device = context;
    return device != NULL && port != NULL && device->dma == port->context &&
           port->ops == &nx_gd32_spi_dma_ops && device->dma != NULL &&
           device->endpoint.port == &device->dma->spi;
}

/** \brief One immutable methods table per finite full-duplex controller mode.
 */
const nx_spi_ops_t nx_gd32_spi_dma_ops = {
    .recover = recover,
    .cancel = cancel,
    .service = service,
    .stop = stop,
    .attach_wake = attach_wake,
};

/** \brief Static child faces preserve independent CS/mode/rate identity. */
const nx_spi_endpoint_ops_t nx_gd32_spi_dma_endpoint_ops = {
    .transfer = transfer,
    .submit = submit,
    .start_admitted = start_admitted,
    .on_port = on_port,
};
