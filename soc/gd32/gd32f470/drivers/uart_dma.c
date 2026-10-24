/**
 * \file            uart_dma.c
 *
 * \brief           Static UART TX DMA with independently proven wire and memory
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
#include "gd32f470_dma.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "private/system.h"

#ifndef NX_GD32_UART_DMA_DISABLE
#if defined(NX_GD32_DMA_DISABLE)
/* Preserve the primary-route legacy test boundary; firmware has no branch. */
#define NX_GD32_UART_DMA_DISABLE(control, mask)                                \
    do {                                                                       \
        if ((control) == &DMA_CH7CTL(DMA1)) {                                  \
            NX_GD32_DMA_DISABLE(mask);                                         \
        } else {                                                               \
            *(control) &= ~(uint32_t)(mask);                                   \
        }                                                                      \
    } while (0)
#else
#define NX_GD32_UART_DMA_DISABLE(control, mask)                                \
    (*(control) &= ~(uint32_t)(mask))
#endif
#endif
#ifndef NX_GD32_UART_DMA_ACK
#define NX_GD32_UART_DMA_ACK(clear, mask) (*(clear) = (uint32_t)(mask))
#endif

/** \brief Exact controller identity selects the reviewed TX engine. */
static volatile uint32_t*
engine_at(const nx_gd32_uart_controller_t* controller) {
    return controller == &nx_gd32_usart0_controller ? &DMA_CH7CTL(DMA1)
                                                    : &DMA_CH6CTL(DMA0);
}

/** \brief Direct vendor register arithmetic uses the one UART identity. */
static volatile uint32_t* engine(const nx_gd32_uart_state_t* uart) {
    return engine_at(uart->controller);
}

/** \brief Retain the exact controller clock identity. */
static uint32_t dma_base(const nx_gd32_uart_state_t* uart) {
    return uart->controller == &nx_gd32_usart0_controller ? DMA1 : DMA0;
}

/** \brief Resolve a reviewed publisher before cold acquisition. */
static IRQn_Type dma_irq_at(const nx_gd32_uart_controller_t* controller) {
    return controller == &nx_gd32_usart0_controller ? DMA1_Channel7_IRQn
                                                    : DMA0_Channel6_IRQn;
}

/** \brief Hot dispatch uses the same immutable controller identity. */
static IRQn_Type dma_irq(const nx_gd32_uart_state_t* uart) {
    return dma_irq_at(uart->controller);
}

/** \brief The two exact routes use distinct high status groups. */
static uint32_t flag_shift(const nx_gd32_uart_state_t* uart) {
    return uart->controller == &nx_gd32_usart0_controller ? 22U : 16U;
}

#define TX_CONTROL(s)    (engine(s)[0])
#define TX_COUNT(s)      (engine(s)[1])
#define TX_PERIPHERAL(s) (engine(s)[2])
#define TX_MEMORY(s)     (engine(s)[3])
#define TX_FIFO(s)       (engine(s)[5])

/** \brief Preserve the exact selected source and fresh hardware observations.
 */
static uint32_t flags(nx_gd32_uart_state_t* uart) {
    return (DMA_INTF1(dma_base(uart)) >> flag_shift(uart)) & 0x3DU;
}

/** \brief Preserve the exact selected source and fresh hardware observations.
 */
static void acknowledge_bits(nx_gd32_uart_state_t* uart, uint32_t observed) {
    NX_GD32_UART_DMA_ACK(&DMA_INTC1(dma_base(uart)),
                         observed << flag_shift(uart));
    NVIC_ClearPendingIRQ(dma_irq(uart));
}

/** \brief Preserve the exact selected source and fresh hardware observations.
 */
static void acknowledge(nx_gd32_uart_state_t* uart) {
    acknowledge_bits(uart, 0x3DU);
}

/** \brief Preserve the exact selected source and fresh hardware observations.
 */
static void detach(nx_gd32_uart_state_t* uart) {
    USART_CTL2(uart->controller->registers) &= ~(uint32_t)USART_CTL2_DENT;
    NX_GD32_UART_DMA_DISABLE(&TX_CONTROL(uart),
                             DMA_CHXCTL_CHEN | DMA_CHXCTL_FTFIE |
                                 DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE |
                                 DMA_CHXCTL_SDEIE);
    nx_gd32_peripheral_barrier();
    /* Completion W1C cannot erase an error arriving during withdrawal. */
    acknowledge_bits(uart, flags(uart) & 0x30U);
}

/** \brief Initialize the sole UART and selected TX extension once. */
nx_result_t nx_gd32_uart_dma_initialize_parts(
    nx_gd32_uart_state_t* uart, const nx_gd32_uart_dma_resources_t* resources,
    nx_gd32_uart_tx_dma_extension_t* transmit,
    const nx_gd32_uart_controller_t* controller, uint32_t baud,
    nx_uart_rx_profile_t profile, void* storage, size_t capacity,
    unsigned priority) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        resources->regions == NULL || resources->region_count == 0U ||
        (controller != &nx_gd32_usart0_controller &&
         controller != &nx_gd32_usart1_controller)) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((*engine_at(controller) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    /* Idle EN does not surrender an already enabled publisher's lease. */
    unsigned vector = (unsigned)dma_irq_at(controller);
    if ((NVIC->ISER[vector / 32U] & (1UL << (vector % 32U))) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_gd32_uart_initialize_at(
        uart, controller, baud, profile, storage, capacity, priority);
    if (result != NX_SUCCESS) {
        return result;
    }
    transmit->dma_complete = false;
    transmit->aborting = false;
    acknowledge(uart);
    NVIC_SetPriority(dma_irq(uart), priority);
    NVIC_EnableIRQ(dma_irq(uart));
    return NX_SUCCESS;
}

/** \brief Check the exact TX path without stores or loans. */
static inline nx_result_t
validate_tx(nx_gd32_uart_state_t* uart,
            const nx_gd32_uart_dma_resources_t* resources,
            nx_gd32_uart_tx_dma_extension_t* transmit,
            const nx_uart_tx_request_t* request) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        request == NULL || request->data == NULL || request->length == 0U ||
        request->length > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = nx_dma_buffer_validate(
        resources->regions, resources->region_count, request->data,
        request->length, 1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!uart->initialized || uart->stopping) {
        return NX_ERROR_STATE;
    }
    if (uart->active != NULL || (TX_CONTROL(uart) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

/** \brief Admit only after the selected DMA domain checks. */
static nx_result_t start(nx_gd32_uart_state_t* uart,
                         const nx_gd32_uart_dma_resources_t* resources,
                         nx_gd32_uart_tx_dma_extension_t* transmit,
                         nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t result = validate_tx(uart, resources, transmit, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t saved = nx_gd32_critical_enter();
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_gd32_critical_leave(saved);
        return result;
    }
    uart->active = request;
    uart->tx_position = 0U;
    uart->tc = false;
    uart->terminal = NX_SUCCESS;
    transmit->drain_deadline = NX_DEADLINE_NEVER;
    transmit->dma_complete = false;
    transmit->aborting = false;
    acknowledge(uart);
    TX_PERIPHERAL(uart) =
        (uint32_t)(uintptr_t)&USART_DATA(uart->controller->registers);
    TX_MEMORY(uart) = (uint32_t)(uintptr_t)request->data;
    TX_COUNT(uart) = (uint32_t)request->length;
    TX_FIFO(uart) = 0U;
    TX_CONTROL(uart) = (4U << 25U) | DMA_CHXCTL_MNAGA | (1U << 6U) |
                       (1U << 17U) | DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE |
                       DMA_CHXCTL_SDEIE;
    USART_CTL0(uart->controller->registers) &=
        ~(uint32_t)(USART_CTL0_TBEIE | USART_CTL0_TCIE);
    USART_STAT0(uart->controller->registers) &= ~(uint32_t)USART_STAT0_TC;
    nx_gd32_peripheral_barrier();
    TX_CONTROL(uart) |= DMA_CHXCTL_CHEN;
    USART_CTL2(uart->controller->registers) |= USART_CTL2_DENT;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief Retain the unique borrow through independently proven memory and wire
 * drain. */
static void begin_drain(nx_gd32_uart_state_t* uart,
                        nx_gd32_uart_tx_dma_extension_t* transmit,
                        nx_result_t terminal) {
    detach(uart);
    transmit->aborting = true;
    uart->terminal = terminal;
    (void)nx_request_transition(&uart->active->base, NX_REQUEST_DRAINING);
    transmit->drain_deadline =
        nx_deadline_after(nx_time_now_us(), 20000000U / uart->baud + 100U);
    bool untouched = (TX_CONTROL(uart) & DMA_CHXCTL_CHEN) == 0U &&
                     TX_COUNT(uart) == uart->active->length;
    uart->tc = untouched || (USART_STAT0(uart->controller->registers) &
                             USART_STAT0_TC) != 0U;
    if (!uart->tc) {
        USART_CTL0(uart->controller->registers) |= USART_CTL0_TCIE;
    }
}

/** \brief Cancellation never revokes the caller loan. */
nx_result_t nx_gd32_uart_dma_cancel_parts(
    nx_gd32_uart_state_t* uart, const nx_gd32_uart_dma_resources_t* resources,
    nx_gd32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (uart->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_gd32_critical_leave(saved);
        return result;
    }
    if (!uart->tc && !transmit->aborting) {
        begin_drain(uart, transmit, NX_ERROR_CANCELLED);
    }
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief DMA completion enables separate UART wire completion. */
void nx_gd32_uart_dma_irq_parts(nx_gd32_uart_state_t* uart,
                                const nx_gd32_uart_dma_resources_t* resources,
                                nx_gd32_uart_tx_dma_extension_t* transmit) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        !uart->initialized) {
        return;
    }
    uint32_t status = flags(uart);
    if ((status & 0x2DU) == 0U) {
        return;
    }
    if (uart->active == NULL) {
        acknowledge(uart);
        return;
    }
    if (!transmit->aborting && (status & 0x0DU) != 0U) {
        begin_drain(uart, transmit, NX_ERROR_IO);
        (void)nx_irq_wake_signal(uart->wake);
    } else if (!transmit->aborting && !transmit->dma_complete) {
        if ((status & 0x0DU) != 0U || TX_COUNT(uart) != 0U) {
            begin_drain(uart, transmit, NX_ERROR_IO);
        } else {
            uart->tx_position = uart->active->length;
            detach(uart);
            if ((flags(uart) & 0x0DU) != 0U) {
                begin_drain(uart, transmit, NX_ERROR_IO);
            } else {
                transmit->dma_complete = true;
                transmit->drain_deadline = nx_deadline_after(
                    nx_time_now_us(), 20000000U / uart->baud + 100U);
                USART_CTL0(uart->controller->registers) |= USART_CTL0_TCIE;
            }
        }
        (void)nx_irq_wake_signal(uart->wake);
    } else {
        acknowledge_bits(uart, flags(uart) & 0x30U);
    }
}

/** \brief Preserve fresh terminal errors through final settlement. */
void nx_gd32_uart_dma_service_parts(
    nx_gd32_uart_state_t* uart, const nx_gd32_uart_dma_resources_t* resources,
    nx_gd32_uart_tx_dma_extension_t* transmit) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_uart_tx_request_t* request = uart->active;
    if (request == NULL) {
        nx_gd32_critical_leave(saved);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    if (!transmit->aborting && (flags(uart) & 0x0DU) != 0U) {
        begin_drain(uart, transmit, NX_ERROR_IO);
    }
    if (!uart->tc && !transmit->aborting &&
        nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(uart, transmit, NX_ERROR_TIMEOUT);
    }
    nx_dma_drain_facts_t facts = {
        .length = request->length,
        .remaining = TX_COUNT(uart),
        .engine_disabled = (TX_CONTROL(uart) & DMA_CHXCTL_CHEN) == 0U,
        .irq_detached =
            (TX_CONTROL(uart) & (DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE |
                                 DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE)) == 0U,
        .peripheral_idle = uart->tc,
    };
    if (nx_dma_drain_check(&facts) == NX_SUCCESS) {
        USART_CTL0(uart->controller->registers) &=
            ~(uint32_t)(USART_CTL0_TCIE | USART_CTL0_TBEIE);
        acknowledge_bits(uart, flags(uart) & 0x30U);
        /* Actual EN/IRQ withdrawal precedes this final error observation. */
        if (uart->terminal == NX_SUCCESS && (flags(uart) & 0x0DU) != 0U) {
            uart->terminal = NX_ERROR_IO;
        }
        size_t transferred =
            transmit->dma_complete && uart->terminal == NX_SUCCESS
                ? request->length
                : 0U;
        nx_result_t terminal = uart->terminal;
        uart->active = NULL;
        nx_request_settle(&request->base, terminal, transferred);
    } else {
        if (transmit->dma_complete && !transmit->aborting &&
            nx_deadline_expired(transmit->drain_deadline, now)) {
            transmit->aborting = true;
            uart->terminal = NX_ERROR_IO;
            (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
            detach(uart);
        }
        if (transmit->aborting &&
            nx_deadline_expired(transmit->drain_deadline, now) &&
            nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
    nx_gd32_critical_leave(saved);
}

/** \brief Quiesce the extension without releasing its shared UART owner. */
nx_result_t
nx_gd32_uart_dma_stop_parts(nx_gd32_uart_state_t* uart,
                            const nx_gd32_uart_dma_resources_t* resources,
                            nx_gd32_uart_tx_dma_extension_t* transmit) {
    if (uart == NULL || resources == NULL || transmit == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!uart->initialized || uart->controller == NULL) {
        return NX_ERROR_STATE;
    }
    uart->stopping = true;
    if (uart->active != NULL) {
        (void)nx_gd32_uart_dma_cancel_parts(uart, resources, transmit,
                                            uart->active);
        nx_gd32_uart_dma_service_parts(uart, resources, transmit);
        if (uart->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    detach(uart);
    if ((TX_CONTROL(uart) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    NVIC_DisableIRQ(dma_irq(uart));
    NVIC_ClearPendingIRQ(dma_irq(uart));
    return NX_SUCCESS;
}

/** \brief Direct admission uses explicit selected components. */
nx_result_t nx_gd32_uart_dma_submit_parts(
    nx_gd32_uart_state_t* uart, const nx_gd32_uart_dma_resources_t* resources,
    nx_gd32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request) {
    return start(uart, resources, transmit, request, false);
}

/** \brief Transfer an adapter loan without readmission. */
nx_result_t nx_gd32_uart_dma_start_admitted_parts(
    nx_gd32_uart_state_t* uart, const nx_gd32_uart_dma_resources_t* resources,
    nx_gd32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(uart, resources, transmit, request, true);
}

/** \brief Expose the actual selected pure TX validator. */
nx_result_t nx_gd32_uart_dma_tx_validate_parts(
    nx_gd32_uart_state_t* uart, const nx_gd32_uart_dma_resources_t* resources,
    nx_gd32_uart_tx_dma_extension_t* transmit,
    const nx_uart_tx_request_t* request) {
    return validate_tx(uart, resources, transmit, request);
}

/** \brief Latch TX facts without reading DATA owned by live RX DMA. */
bool nx_gd32_uart_dma_wire_irq_parts(nx_gd32_uart_state_t* uart,
                                     nx_gd32_uart_tx_dma_extension_t* transmit,
                                     uint32_t status) {
    if (uart == NULL || transmit == NULL || !uart->initialized) {
        return false;
    }
    bool notify = false;
    if (transmit->aborting && uart->active != NULL &&
        (status & USART_STAT0_TC) != 0U) {
        uart->tc = true;
        USART_CTL0(uart->controller->registers) &= ~(uint32_t)USART_CTL0_TCIE;
        notify = true;
    }
    return nx_gd32_uart_tx_irq(uart, status) || notify;
}

/** \brief Decode the statically selected finite TX owner. */
nx_result_t
nx_gd32_uart_dma_initialize_at(nx_gd32_uart_dma_state_t* state,
                               const nx_gd32_uart_controller_t* controller,
                               uint32_t baud, nx_uart_rx_profile_t profile,
                               void* storage, size_t capacity,
                               unsigned priority) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_gd32_uart_dma_initialize_parts(
        &state->uart, &state->resources, &state->transmit, controller, baud,
        profile, storage, capacity, priority);
}

/** \brief Retain the reviewed primary route initializer. */
nx_result_t nx_gd32_uart_dma_initialize(nx_gd32_uart_dma_state_t* state,
                                        uint32_t baud,
                                        nx_uart_rx_profile_t profile,
                                        void* storage, size_t capacity,
                                        unsigned priority) {
    return nx_gd32_uart_dma_initialize_at(state, &nx_gd32_usart0_controller,
                                          baud, profile, storage, capacity,
                                          priority);
}

/** \brief Decode exact components from the immutable finite TX face. */
nx_result_t nx_gd32_uart_dma_submit(void* context,
                                    nx_uart_tx_request_t* request) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_gd32_uart_dma_submit_parts(&state->uart, &state->resources,
                                         &state->transmit, request);
}

/** \brief Decode exact components from the immutable finite TX face. */
nx_result_t nx_gd32_uart_dma_start_admitted(void* context,
                                            nx_uart_tx_request_t* request) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_gd32_uart_dma_start_admitted_parts(
        &state->uart, &state->resources, &state->transmit, request);
}

/** \brief Decode exact components from the immutable finite TX face. */
nx_result_t nx_gd32_uart_dma_cancel(void* context,
                                    nx_uart_tx_request_t* request) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_gd32_uart_dma_cancel_parts(&state->uart, &state->resources,
                                         &state->transmit, request);
}

/** \brief Use the single owner and selected extension without stored
 * indirection. */
void nx_gd32_uart_dma_service(void* context) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return;
    }
    nx_gd32_uart_dma_service_parts(&state->uart, &state->resources,
                                   &state->transmit);
}

/** \brief Use the single owner and selected extension without stored
 * indirection. */
void nx_gd32_uart_dma_irq(nx_gd32_uart_dma_state_t* state) {
    if (state == NULL) {
        return;
    }
    nx_gd32_uart_dma_irq_parts(&state->uart, &state->resources,
                               &state->transmit);
}

/** \brief Release the UART only after its selected TX extension is quiescent.
 */
nx_result_t nx_gd32_uart_dma_stop(void* context) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_gd32_uart_dma_stop_parts(
        &state->uart, &state->resources, &state->transmit);
    return result == NX_SUCCESS ? nx_gd32_uart_stop(&state->uart) : result;
}

/** \brief Finite TX retains its independent IRQ receive representation. */
void nx_gd32_uart_dma_uart_irq(nx_gd32_uart_dma_state_t* state) {
    if (state != NULL && state->uart.initialized) {
        nx_gd32_uart_state_t* uart = &state->uart;
        if (state->transmit.aborting && uart->active != NULL &&
            (USART_STAT0(uart->controller->registers) & USART_STAT0_TC) != 0U) {
            uart->tc = true;
            USART_CTL0(uart->controller->registers) &= ~USART_CTL0_TCIE;
        }
        nx_gd32_uart_irq(uart);
    }
}

/** \brief Preserve independent byte receive and actual publisher checks. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->uart.initialized || state->uart.controller == NULL) {
        return NX_ERROR_STATE;
    }
    nx_result_t result =
        nx_gd32_irq_wake_validate(wake, dma_irq(&state->uart), syscall_ceiling);
    return result == NX_SUCCESS ? nx_gd32_uart_ops.attach_wake(
                                      &state->uart, wake, syscall_ceiling)
                                : result;
}

/** \brief Preserve independent byte receive and actual publisher checks. */
static nx_result_t read_events(void* context, nx_uart_rx_event_t* events,
                               size_t capacity, size_t* count) {
    nx_gd32_uart_dma_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_gd32_uart_read_events(&state->uart, events,
                                                    capacity, count);
}

/** \brief Preserve independent byte receive and actual publisher checks. */
static nx_result_t read_bytes(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    nx_gd32_uart_dma_state_t* state = context;
    return state == NULL
               ? NX_ERROR_INVALID
               : nx_gd32_uart_read_bytes(&state->uart, bytes, capacity, count);
}

/** \brief Shared immutable finite TX DMA operations. */
const nx_uart_ops_t nx_gd32_uart_dma_ops = {
    .submit = nx_gd32_uart_dma_submit,
    .start_admitted = nx_gd32_uart_dma_start_admitted,
    .cancel = nx_gd32_uart_dma_cancel,
    .service = nx_gd32_uart_dma_service,
    .read_events = read_events,
    .read_bytes = read_bytes,
    .stop = nx_gd32_uart_dma_stop,
    .attach_wake = attach_wake,
};

/** \brief Expose the matching finite TX admission validator. */
nx_result_t nx_gd32_uart_dma_tx_validate(void* context,
                                         const nx_uart_tx_request_t* request) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_gd32_uart_dma_tx_validate_parts(&state->uart, &state->resources,
                                              &state->transmit, request);
}
