/**
 * \file            spi_dma.c
 * \brief           Selected finite DMA with independent memory and wire drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_spi_dma.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "private/system.h"
#include "spi_dma_sequence_owner.h"

#ifndef NX_GD32_SPI_DMA_CONTROL_DISABLE
#if defined(NX_GD32_SPI_DMA_DISABLE)
/* Existing primary-route fault fixtures remain an external test boundary. */
#define NX_GD32_SPI_DMA_CONTROL_DISABLE(control, mask)                         \
    do {                                                                       \
        if ((control) == &DMA_CH3CTL(DMA1) ||                                  \
            (control) == &DMA_CH4CTL(DMA1)) {                                  \
            NX_GD32_SPI_DMA_DISABLE((control) == &DMA_CH3CTL(DMA1), (mask));   \
        } else {                                                               \
            *(control) &= ~(uint32_t)(mask);                                   \
        }                                                                      \
    } while (0)
#else
#define NX_GD32_SPI_DMA_CONTROL_DISABLE(control, mask)                         \
    (*(control) &= ~(uint32_t)(mask))
#endif
#endif
#ifndef NX_GD32_SPI_DMA_FLAGS_ACK
#if defined(NX_GD32_SPI_DMA_ACK)
#define NX_GD32_SPI_DMA_FLAGS_ACK(clear, mask)                                 \
    NX_GD32_SPI_DMA_ACK((clear) == &DMA_INTC0(DMA1), (mask))
#else
#define NX_GD32_SPI_DMA_FLAGS_ACK(clear, mask) (*(clear) = (uint32_t)(mask))
#endif
#endif

#ifndef NX_SPI_DMA_SEQUENCE_ENGINE_READ
#define NX_SPI_DMA_SEQUENCE_ENGINE_READ(control) (*(control))
#endif

/** \brief Maintained identity selects one exact pair without new instance RAM.
 */
static bool primary(const nx_gd32_spi_dma_state_t* state) {
    return state->spi.controller == &nx_gd32_spi4_controller;
}
/** \brief Literal DMA1 physical engines: SPI4 RX3/TX4 or SPI0 RX2/TX5. */
static volatile uint32_t* engine_at(const nx_gd32_spi_controller_t* controller,
                                    bool receive) {
    if (controller == &nx_gd32_spi4_controller) {
        return receive ? &DMA_CH3CTL(DMA1) : &DMA_CH4CTL(DMA1);
    }
    return receive ? &DMA_CH2CTL(DMA1) : &DMA_CH5CTL(DMA1);
}
/** \brief Direct SDK channel layout, no hot runtime operations table. */
static volatile uint32_t* engine(const nx_gd32_spi_dma_state_t* state,
                                 bool receive) {
    return engine_at(state->spi.controller, receive);
}
/** \brief Each physical engine has its own interrupt group and vector. */
static IRQn_Type dma_irq_at(const nx_gd32_spi_controller_t* controller,
                            bool receive) {
    if (controller == &nx_gd32_spi4_controller) {
        return receive ? DMA1_Channel3_IRQn : DMA1_Channel4_IRQn;
    }
    return receive ? DMA1_Channel2_IRQn : DMA1_Channel5_IRQn;
}
/** \brief Hot dispatch retains the same reviewed cold publisher identity. */
static IRQn_Type dma_irq(const nx_gd32_spi_dma_state_t* state, bool receive) {
    return dma_irq_at(state->spi.controller, receive);
}
/** \brief RX uses INTF0 and TX uses INTF1 for both reviewed pairs. */
static uint32_t flag_shift(const nx_gd32_spi_dma_state_t* state, bool receive) {
    if (primary(state))
        return receive ? 22U : 0U;
    return receive ? 16U : 6U;
}
/** \brief Read only the selected engine's terminal group. */
static uint32_t flags(const nx_gd32_spi_dma_state_t* state, bool receive) {
    uint32_t bank = receive ? DMA_INTF0(DMA1) : DMA_INTF1(DMA1);
    return (bank >> flag_shift(state, receive)) & 0x3DU;
}
/** \brief W1C preserves the other controller's group in the shared DMA1 bank.
 */
static void acknowledge_bits(nx_gd32_spi_dma_state_t* state, bool receive,
                             uint32_t observed) {
    volatile uint32_t* clear = receive ? &DMA_INTC0(DMA1) : &DMA_INTC1(DMA1);
    NX_GD32_SPI_DMA_FLAGS_ACK(clear, observed << flag_shift(state, receive));
    NVIC_ClearPendingIRQ(dma_irq(state, receive));
}
/** \brief Cold/reset cleanup owns the selected stale flag group. */
static void acknowledge(nx_gd32_spi_dma_state_t* state, bool receive) {
    acknowledge_bits(state, receive, 0x3DU);
}

/** \brief Withdraw only the selected request and memory engine before drain. */
static void detach(nx_gd32_spi_dma_state_t* state, bool receive) {
    SPI_CTL1(state->spi.controller->registers) &=
        ~(uint32_t)(receive ? SPI_CTL1_DMAREN : SPI_CTL1_DMATEN);
    NX_GD32_SPI_DMA_CONTROL_DISABLE(&engine(state, receive)[0],
                                    DMA_CHXCTL_CHEN | DMA_CHXCTL_FTFIE |
                                        DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE |
                                        DMA_CHXCTL_SDEIE);
    nx_gd32_peripheral_barrier();
    /* Preserve errors first observed after the IRQ terminal snapshot. */
    acknowledge_bits(state, receive, flags(state, receive) & 0x30U);
}

/** \brief Acquire one exact controller and its two reviewed DMA publishers. */
nx_result_t nx_gd32_spi_dma_initialize_at(
    nx_gd32_spi_dma_state_t* state, nx_gd32_spi_dma_endpoint_state_t* endpoint,
    const nx_gd32_spi_controller_t* controller, uint32_t cs_gpio,
    uint32_t cs_mask, uint32_t maximum_hz, unsigned mode, unsigned priority) {
    if (state == NULL || endpoint == NULL || state->regions == NULL ||
        state->region_count == 0U || priority > 15U ||
        (controller != &nx_gd32_spi4_controller &&
         controller != &nx_gd32_spi0_controller)) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->initialized || state->spi.initialized || state->active != NULL ||
        ((*engine_at(controller, true) | *engine_at(controller, false)) &
         DMA_CHXCTL_CHEN) != 0U ||
        (SPI_STAT(controller->registers) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    /* An idle engine does not release another owner's enabled DMA vector. */
    unsigned rx_vector = (unsigned)dma_irq_at(controller, true);
    unsigned tx_vector = (unsigned)dma_irq_at(controller, false);
    if ((NVIC->ISER[rx_vector / 32U] & (1UL << (rx_vector % 32U))) != 0U ||
        (NVIC->ISER[tx_vector / 32U] & (1UL << (tx_vector % 32U))) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result =
        nx_gd32_spi_initialize_at(&state->spi, &endpoint->endpoint, controller,
                                  cs_gpio, cs_mask, maximum_hz, mode);
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
    acknowledge(state, true);
    acknowledge(state, false);
    NVIC_SetPriority(dma_irq(state, true), priority);
    NVIC_SetPriority(dma_irq(state, false), priority);
    NVIC_EnableIRQ(dma_irq(state, true));
    NVIC_EnableIRQ(dma_irq(state, false));
    return NX_SUCCESS;
}

/** \brief Primary initializer remains a cold wrapper with unchanged storage. */
nx_result_t nx_gd32_spi_dma_initialize(
    nx_gd32_spi_dma_state_t* state, nx_gd32_spi_dma_endpoint_state_t* endpoint,
    uint32_t cs_gpio, uint32_t cs_mask, uint32_t maximum_hz, unsigned mode,
    unsigned priority) {
    return nx_gd32_spi_dma_initialize_at(state, endpoint,
                                         &nx_gd32_spi4_controller, cs_gpio,
                                         cs_mask, maximum_hz, mode, priority);
}

/** \brief Validate immutable child identity and CS without touching hardware.
 */
static nx_result_t
configuration(const nx_gd32_spi_dma_endpoint_state_t* device) {
    if (device == NULL || device->dma == NULL ||
        device->endpoint.port != &device->dma->spi ||
        (device->dma->spi.controller != &nx_gd32_spi4_controller &&
         device->dma->spi.controller != &nx_gd32_spi0_controller) ||
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
                                    const nx_spi_request_t* request,
                                    uint8_t memory_flags, uint8_t bits) {
    if ((bits != 8U && bits != 16U) ||
        (memory_flags & ~(NX_SPI_MEMORY_TX_FIXED | NX_SPI_MEMORY_RX_FIXED)) !=
            0U) {
        return NX_ERROR_INVALID;
    }
    if (request == NULL || request->length == 0U || request->length > 65535U) {
        return NX_ERROR_INVALID;
    }
    if (request->tx == NULL || request->rx == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t width = bits / 8U;
    size_t tx_span = (memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U
                         ? width
                         : request->length * width;
    size_t rx_span = (memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U
                         ? width
                         : request->length * width;
    nx_result_t result =
        nx_dma_buffer_validate(state->regions, state->region_count, request->tx,
                               tx_span, width, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    result =
        nx_dma_buffer_validate(state->regions, state->region_count, request->rx,
                               rx_span, width, NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    uintptr_t tx = (uintptr_t)request->tx;
    uintptr_t rx = (uintptr_t)request->rx;
    return tx < rx + rx_span && rx < tx + tx_span ? NX_ERROR_INVALID
                                                  : NX_SUCCESS;
}

/** \brief Program selected disabled engines and explicit fixed/increment spans.
 */
static void program(nx_gd32_spi_dma_state_t* state,
                    const nx_spi_request_t* request, bool receive,
                    uint8_t memory_flags, uint8_t bits) {
    acknowledge(state, receive);
    uint8_t fixed = receive ? NX_SPI_MEMORY_RX_FIXED : NX_SPI_MEMORY_TX_FIXED;
    uint32_t increment = (memory_flags & fixed) != 0U ? 0U : DMA_CHXCTL_MNAGA;
    uint32_t control = (bits == 16U ? (1U << 11U) | (1U << 13U) : 0U) |
                       ((primary(state) ? 2U : 3U) << 25U) | increment |
                       (1U << 17U) | DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE |
                       DMA_CHXCTL_SDEIE | (receive ? 0U : 1U << 6U);
    if (receive) {
        engine(state, true)[2] =
            (uint32_t)(uintptr_t)&SPI_DATA(state->spi.controller->registers);
        engine(state, true)[3] = (uint32_t)(uintptr_t)request->rx;
        engine(state, true)[1] = (uint32_t)request->length;
        engine(state, true)[5] = 0U;
        engine(state, true)[0] = control;
    } else {
        engine(state, false)[2] =
            (uint32_t)(uintptr_t)&SPI_DATA(state->spi.controller->registers);
        engine(state, false)[3] = (uint32_t)(uintptr_t)request->tx;
        engine(state, false)[1] = (uint32_t)request->length;
        engine(state, false)[5] = 0U;
        engine(state, false)[0] = control;
    }
}

/** \brief One successful admission gates both engines and the whole CS
 * interval. */
static nx_result_t start(nx_gd32_spi_dma_endpoint_state_t* device,
                         nx_spi_request_t* request, bool admitted,
                         uint8_t memory_flags, uint8_t bits, bool retained_cs) {
    nx_result_t result = configuration(device);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_gd32_spi_dma_state_t* state = device->dma;
    result = validate_buffers(state, request, memory_flags, bits);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t saved = nx_gd32_critical_enter();
    /* Invalid state/lease defaults to STATE; physical BUSY keeps priority. */
    result = NX_ERROR_STATE;
    if (state->initialized && !state->stopping && !state->faulted) {
        if (state->active != NULL || (state->spi.active && !retained_cs) ||
            ((engine(state, true)[0] | engine(state, false)[0]) &
             DMA_CHXCTL_CHEN) != 0U ||
            (SPI_STAT(state->spi.controller->registers) & SPI_STAT_TRANS) !=
                0U) {
            result = NX_ERROR_BUSY;
        } else if (!retained_cs ||
                   (state->spi.active && state->endpoint == device)) {
            if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
                result = NX_ERROR_TIMEOUT;
            } else {
                result = admitted ? nx_request_transition(&request->base,
                                                          NX_REQUEST_ACTIVE)
                                  : nx_request_admit(&request->base,
                                                     NX_REQUEST_ACTIVE);
            }
        }
    }
    if (result != NX_SUCCESS) {
        nx_gd32_critical_leave(saved);
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
    SPI_CTL0(state->spi.controller->registers) =
        device->endpoint.control | (bits == 16U ? SPI_CTL0_FF16 : 0U);
    SPI_CTL1(state->spi.controller->registers) = 0U;
    program(state, request, true, memory_flags, bits);
    program(state, request, false, memory_flags, bits);
    if (!retained_cs) {
        GPIO_BOP(device->endpoint.cs_gpio) = device->endpoint.cs_mask << 16U;
    }
    nx_gd32_peripheral_barrier();
    engine(state, true)[0] |= DMA_CHXCTL_CHEN;
    engine(state, false)[0] |= DMA_CHXCTL_CHEN;
    SPI_CTL1(state->spi.controller->registers) =
        SPI_CTL1_DMAREN | SPI_CTL1_DMATEN;
    SPI_CTL0(state->spi.controller->registers) |= SPI_CTL0_SPIEN;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief Rejectable conditions are resolved before the provider creates a
 * loan. */
static nx_result_t submit(void* context, nx_spi_request_t* request) {
    return start(context, request, false, 0U, 8U, false);
}

/** \brief Copy explicit memory modes only after valid request admission. */
static nx_result_t submit_memory(void* context, nx_spi_request_t* request,
                                 uint8_t memory_flags) {
    return start(context, request, false, memory_flags, 8U, false);
}

/** \brief Transfer an existing adapter loan without second admission. */
static nx_result_t start_admitted(void* context, nx_spi_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(context, request, true, 0U, 8U, false);
}

/** \brief Withdraw both memory sources while preserving CS and all references.
 */
static void begin_drain(nx_gd32_spi_dma_state_t* state, nx_result_t terminal) {
    detach(state, false);
    detach(state, true);
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
    uint32_t status = flags(state, receive);
    if ((status & 0x2DU) == 0U) {
        nx_gd32_critical_leave(saved);
        return;
    }
    if (state->active == NULL || state->draining ||
        (receive ? state->rx_complete : state->tx_complete)) {
        if (state->active == NULL) {
            acknowledge(state, receive);
        } else {
            acknowledge_bits(state, receive, status & 0x30U);
        }
        nx_gd32_critical_leave(saved);
        return;
    }
    uint32_t remaining =
        receive ? engine(state, true)[1] : engine(state, false)[1];
    if ((status & 0x0DU) != 0U || remaining != 0U) {
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
static void service_phase(void* context, bool retained_cs) {
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
    if (!state->draining &&
        ((flags(state, true) | flags(state, false)) & 0x0DU) != 0U) {
        begin_drain(state, NX_ERROR_IO);
    }
    uint32_t status = SPI_STAT(state->spi.controller->registers);
    if (!state->draining && (status & (SPI_STAT_CONFERR | SPI_STAT_RXORERR |
                                       SPI_STAT_FERR)) != 0U) {
        begin_drain(state, NX_ERROR_IO);
    } else if (!state->draining &&
               nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT);
    }
    bool terminal =
        state->draining || (state->tx_complete && state->rx_complete);
    uint32_t irq_mask = DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE |
                        DMA_CHXCTL_SDEIE;
    /* Disabled readback can coincide with the final peripheral bus beat.
     * Sequence every engine/IRQ proof before the final wire-idle snapshot. */
    uint32_t requests = SPI_CTL1(state->spi.controller->registers);
    uint32_t tx_control =
        NX_SPI_DMA_SEQUENCE_ENGINE_READ(&engine(state, false)[0]);
    uint32_t rx_control =
        NX_SPI_DMA_SEQUENCE_ENGINE_READ(&engine(state, true)[0]);
    size_t tx_remaining = engine(state, false)[1];
    size_t rx_remaining = engine(state, true)[1];
    bool requests_detached =
        (requests & (SPI_CTL1_DMAREN | SPI_CTL1_DMATEN)) == 0U;
    nx_gd32_peripheral_barrier();
    status = SPI_STAT(state->spi.controller->registers);
    bool idle =
        (status & SPI_STAT_TRANS) == 0U && (status & SPI_STAT_TBE) != 0U;
    nx_dma_drain_facts_t tx = {
        .length = request->length,
        .remaining = tx_remaining,
        .engine_disabled = (tx_control & DMA_CHXCTL_CHEN) == 0U,
        .irq_detached = (tx_control & irq_mask) == 0U,
        .peripheral_idle = idle,
    };
    nx_dma_drain_facts_t rx = tx;
    rx.remaining = rx_remaining;
    rx.engine_disabled = (rx_control & DMA_CHXCTL_CHEN) == 0U;
    rx.irq_detached = (rx_control & irq_mask) == 0U;
    if (terminal && requests_detached &&
        nx_dma_drain_check(&tx) == NX_SUCCESS &&
        nx_dma_drain_check(&rx) == NX_SUCCESS) {
        SPI_CTL1(state->spi.controller->registers) = 0U;
        SPI_CTL0(state->spi.controller->registers) &= ~(uint32_t)SPI_CTL0_SPIEN;
        (void)SPI_DATA(state->spi.controller->registers);
        (void)SPI_STAT(state->spi.controller->registers);
        nx_gd32_peripheral_barrier();
        const nx_gd32_spi_endpoint_state_t* endpoint =
            &state->endpoint->endpoint;
        if (!retained_cs) {
            GPIO_BOP(endpoint->cs_gpio) = endpoint->cs_mask;
        }
        nx_gd32_peripheral_barrier();
        /* Both engines are actually disabled; do not erase a late error. */
        if (state->terminal == NX_SUCCESS &&
            ((flags(state, true) | flags(state, false)) & 0x0DU) != 0U) {
            state->terminal = NX_ERROR_IO;
        }
        nx_result_t result = state->terminal;
        size_t count = result == NX_SUCCESS ? request->length : 0U;
        state->faulted = result == NX_ERROR_IO;
        state->active = NULL;
        if (!retained_cs) {
            state->endpoint = NULL;
        }
        state->spi.active = retained_cs;
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

/** \brief Legacy requests release their own complete CS interval. */
static void service(void* context) {
    service_phase(context, false);
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
    detach(state, false);
    detach(state, true);
    if (((engine(state, true)[0] | engine(state, false)[0]) &
         DMA_CHXCTL_CHEN) != 0U ||
        (SPI_STAT(state->spi.controller->registers) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_gd32_spi_stop(&state->spi);
    if (result != NX_SUCCESS) {
        return result;
    }
    NVIC_DisableIRQ(dma_irq(state, true));
    NVIC_DisableIRQ(dma_irq(state, false));
    NVIC_ClearPendingIRQ(dma_irq(state, true));
    NVIC_ClearPendingIRQ(dma_irq(state, false));
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
        ((engine(state, true)[0] | engine(state, false)[0]) &
         DMA_CHXCTL_CHEN) != 0U) {
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
    nx_result_t result =
        nx_gd32_irq_wake_validate(wake, dma_irq(state, true), syscall_ceiling);
    if (result == NX_SUCCESS) {
        result = nx_gd32_irq_wake_validate(wake, dma_irq(state, false),
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
    .submit_memory = submit_memory,
    .start_admitted = start_admitted,
    .on_port = on_port,
};

/** \brief Check every segment domain before first DMA acquisition or CS. */
static nx_result_t
sequence_validate(void* context, void* child,
                  const nx_spi_dma_transaction_request_t* request) {
    nx_gd32_spi_dma_state_t* state = context;
    nx_gd32_spi_dma_endpoint_state_t* endpoint = child;
    nx_result_t result = configuration(endpoint);
    if (result != NX_SUCCESS || endpoint->dma != state) {
        return result != NX_SUCCESS ? result : NX_ERROR_INVALID;
    }
    if (!state->initialized || state->stopping || state->faulted) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL || state->spi.active ||
        ((engine(state, true)[0] | engine(state, false)[0]) &
         DMA_CHXCTL_CHEN) != 0U ||
        (SPI_STAT(state->spi.controller->registers) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (state->region_count > SIZE_MAX / sizeof(*state->regions)) {
        return NX_ERROR_INVALID;
    }
    result =
        nx_spi_dma_sequence_storage_validate(request, state, sizeof(*state));
    if (result == NX_SUCCESS) {
        result = nx_spi_dma_sequence_storage_validate(request, endpoint,
                                                      sizeof(*endpoint));
    }
    if (result == NX_SUCCESS) {
        result = nx_spi_dma_sequence_storage_validate(
            request, state->regions,
            state->region_count * sizeof(*state->regions));
    }
    if (result == NX_SUCCESS) {
        result = nx_spi_dma_sequence_storage_validate(
            request, state->spi.controller, sizeof(*state->spi.controller));
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    for (size_t i = 0U; i < request->count; ++i) {
        const nx_spi_dma_segment_t* segment = &request->segments[i];
        nx_spi_request_t phase = {
            .tx = segment->tx, .rx = segment->rx, .length = segment->words};
        result = validate_buffers(state, &phase, segment->memory_flags,
                                  request->word_bits);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return NX_SUCCESS;
}

/** \brief Start on exactly the retained physical child without toggling CS. */
static nx_result_t sequence_begin(void* context, void* child,
                                  nx_spi_request_t* phase, uint8_t bits,
                                  uint8_t memory_flags, bool retained_cs) {
    nx_gd32_spi_dma_endpoint_state_t* endpoint = child;
    if (endpoint == NULL || endpoint->dma != context) {
        return NX_ERROR_INVALID;
    }
    return start(endpoint, phase, false, memory_flags, bits, retained_cs);
}

/** \brief Phase settlement preserves the outer CS and complete segment loan. */
static void sequence_advance(void* context) {
    service_phase(context, true);
}

/** \brief Final release requires withdrawn DMA and actual wire idle. */
static nx_result_t sequence_release(void* context, void* child) {
    nx_gd32_spi_dma_state_t* state = context;
    nx_gd32_spi_dma_endpoint_state_t* endpoint = child;
    if (state == NULL || endpoint == NULL || endpoint->dma != state ||
        state->endpoint != endpoint) {
        return NX_ERROR_INVALID;
    }
    if (state->active != NULL ||
        ((engine(state, true)[0] | engine(state, false)[0]) &
         DMA_CHXCTL_CHEN) != 0U ||
        (SPI_STAT(state->spi.controller->registers) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_gd32_peripheral_barrier();
    GPIO_BOP(endpoint->endpoint.cs_gpio) = endpoint->endpoint.cs_mask;
    nx_gd32_peripheral_barrier();
    state->endpoint = NULL;
    state->spi.active = false;
    return NX_SUCCESS;
}

/** \brief Optional mode alone adds the sequence owner, no legacy state fields.
 */
const nx_spi_dma_sequence_backend_t nx_gd32_spi_dma_sequence_backend = {
    .validate = sequence_validate,
    .begin = sequence_begin,
    .advance = sequence_advance,
    .cancel = cancel,
    .release = sequence_release,
    .stop = stop,
};

/** \brief Prove physical endpoint ownership after canonical logical identity.
 */
bool nx_gd32_spi_dma_owner_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port) {
    if (!nx_spi_dma_sequence_contains(endpoint, port)) {
        return false;
    }
    const nx_spi_dma_sequence_state_t* sequence = port->context;
    const nx_spi_dma_sequence_endpoint_t* child = endpoint->context;
    if (sequence->backend != &nx_gd32_spi_dma_sequence_backend ||
        sequence->context == NULL) {
        return false;
    }
    const nx_gd32_spi_dma_state_t* state = sequence->context;
    const nx_gd32_spi_dma_endpoint_state_t* device = child->endpoint;
    return device->dma == state && device->endpoint.port == &state->spi;
}
/** \brief Reuse both actual DMA IRQ policy checks without exposing byte IO. */
nx_result_t
nx_gd32_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t* port,
                                  const nx_irq_wake_t* wake,
                                  uint8_t syscall_ceiling) {
    if (port == NULL || port->ops != &nx_spi_dma_sequence_port_ops ||
        port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    const nx_spi_dma_sequence_state_t* sequence = port->context;
    if (sequence->backend != &nx_gd32_spi_dma_sequence_backend ||
        sequence->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (sequence->stopping || sequence->stopped) {
        return NX_ERROR_STATE;
    }
    return attach_wake(sequence->context, wake, syscall_ceiling);
}
