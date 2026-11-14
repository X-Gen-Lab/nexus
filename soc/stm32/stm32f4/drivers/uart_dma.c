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
#include "stm32f407_dma.h"
#include "stm32f407_uart_hardware.h"
#include "stm32f4xx_ll_usart.h"

#ifndef NX_STM32_DMA_DISABLE
#define NX_STM32_DMA_DISABLE(stream, mask) ((stream)->CR &= ~(uint32_t)(mask))
#endif
#ifndef NX_STM32_UART_DMA_ACK
#define NX_STM32_UART_DMA_ACK(clear, mask) (*(clear) = (uint32_t)(mask))
#endif

/** \brief Each bank contains stream pairs at shifts 0/6 and 16/22. */
static inline unsigned shift(uint8_t stream) {
    return ((stream & 2U) != 0U ? 16U : 0U) + ((stream & 1U) != 0U ? 6U : 0U);
}

/** \brief Read only the selected transmit lane from its actual status bank. */
static uint32_t flags(const nx_stm32_uart_dma_resources_t* resources) {
    uint32_t status = resources->transmit.lane < 4U
                          ? resources->transmit.dma->LISR
                          : resources->transmit.dma->HISR;
    return (status >> shift(resources->transmit.lane)) & 0x3DU;
}

/** \brief Acknowledge this TX source only. */
static void acknowledge_bits(const nx_stm32_uart_dma_resources_t* resources,
                             uint32_t observed) {
    volatile uint32_t* clear = resources->transmit.lane < 4U
                                   ? &resources->transmit.dma->LIFCR
                                   : &resources->transmit.dma->HIFCR;
    NX_STM32_UART_DMA_ACK(clear, observed << shift(resources->transmit.lane));
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(resources->transmit.irq);
#endif
}

/** \brief Cold cleanup owns the selected complete stale flag group. */
static void acknowledge(const nx_stm32_uart_dma_resources_t* resources) {
    acknowledge_bits(resources, 0x3DU);
}

/** \brief Withdraw request and memory engine before observing drain. */
static void detach(nx_stm32_uart_state_t* uart,
                   const nx_stm32_uart_dma_resources_t* resources) {
    uart->registers->CR3 &= ~(uint32_t)USART_CR3_DMAT;
    NX_STM32_DMA_DISABLE(resources->transmit.stream,
                         DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                             DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
    nx_arch_dsb();
    /* Completion W1C cannot erase an error arriving during withdrawal. */
    acknowledge_bits(resources, flags(resources) & 0x30U);
}

/** \brief Initialize the sole UART owner and its selected TX extension once. */
nx_result_t nx_stm32_uart_dma_initialize_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, uint32_t clock_hz,
    bool blocks) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        resources->transmit.dma == NULL || resources->transmit.stream == NULL ||
        resources->regions == NULL || resources->region_count == 0U) {
        return NX_ERROR_INVALID;
    }
    if (uart->profile == NX_UART_RX_BLOCKS && !blocks) {
        return NX_ERROR_UNSUPPORTED;
    }
    const nx_stm32_uart_hardware_t* selected =
        nx_stm32_uart_hardware_for_irq(uart->irq);
    if (selected == NULL || clock_hz != selected->clock_hz ||
        !nx_stm32_uart_dma_connection_valid(selected, &resources->transmit,
                                            false)) {
        return NX_ERROR_UNSUPPORTED;
    }
#ifndef NEXUS_STM32_MODEL
    if (resources->transmit.dma != selected->dma ||
        uart->registers != selected->registers) {
        return NX_ERROR_UNSUPPORTED;
    }
#endif
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((resources->transmit.stream->CR & DMA_SxCR_EN) != 0U ||
        LL_USART_IsEnabled(uart->registers) != 0U) {
        return NX_ERROR_BUSY;
    }
#ifndef NEXUS_STM32_MODEL
    /* A disabled engine can still belong to another retained IRQ publisher. */
    unsigned vector = (unsigned)resources->transmit.irq;
    if ((NVIC->ISER[vector / 32U] & (1UL << (vector % 32U))) != 0U) {
        return NX_ERROR_BUSY;
    }
#endif
    nx_result_t result = nx_stm32_uart_initialize(uart, clock_hz);
    if (result != NX_SUCCESS) {
        return result;
    }
    transmit->dma_complete = false;
    transmit->aborting = false;
    acknowledge(resources);
#ifndef NEXUS_STM32_MODEL
    NVIC_SetPriority(resources->transmit.irq, 5U);
    NVIC_EnableIRQ(resources->transmit.irq);
#endif
    return NX_SUCCESS;
}

/** \brief Check the exact TX path without stores or loans. */
static inline nx_result_t
validate_tx(nx_stm32_uart_state_t* uart,
            const nx_stm32_uart_dma_resources_t* resources,
            nx_stm32_uart_tx_dma_extension_t* transmit,
            const nx_uart_tx_request_t* request) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        request == NULL || request->data == NULL || request->length == 0U ||
        request->length > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!uart->initialized || uart->closing) {
        return NX_ERROR_STATE;
    }
    if (resources->transmit.stream == NULL ||
        nx_stm32_uart_dma_resources_overlap(resources, request,
                                            sizeof(*request)))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        resources->regions, resources->region_count, request->data,
        request->length, 1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (uart->active != NULL ||
        (resources->transmit.stream->CR & DMA_SxCR_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

/** \brief Admit once after all rejectable DMA checks. */
static nx_result_t start(nx_stm32_uart_state_t* uart,
                         const nx_stm32_uart_dma_resources_t* resources,
                         nx_stm32_uart_tx_dma_extension_t* transmit,
                         nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t result = validate_tx(uart, resources, transmit, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    uart->active = request;
    uart->tx_position = 0U;
    uart->tx_complete = false;
    uart->terminal = NX_SUCCESS;
    uart->drain_deadline = NX_DEADLINE_NEVER;
    transmit->dma_complete = false;
    transmit->aborting = false;
    acknowledge(resources);
    resources->transmit.stream->PAR = (uint32_t)(uintptr_t)&uart->registers->DR;
    resources->transmit.stream->M0AR = (uint32_t)(uintptr_t)request->data;
    resources->transmit.stream->NDTR = (uint32_t)request->length;
    resources->transmit.stream->FCR = 0U;
    resources->transmit.stream->CR =
        ((uint32_t)resources->transmit.channel << DMA_SxCR_CHSEL_Pos) |
        DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_PL_1 | DMA_SxCR_TCIE |
        DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    uart->registers->CR1 &= ~(uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE);
    uart->registers->SR &= ~(uint32_t)USART_SR_TC;
    nx_arch_dsb();
    resources->transmit.stream->CR |= DMA_SxCR_EN;
    uart->registers->CR3 |= USART_CR3_DMAT;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Keep the request borrowed while DMA and wire drain independently. */
static void begin_drain(nx_stm32_uart_state_t* uart,
                        const nx_stm32_uart_dma_resources_t* resources,
                        nx_stm32_uart_tx_dma_extension_t* transmit,
                        nx_result_t terminal) {
    detach(uart, resources);
    transmit->aborting = true;
    uart->terminal = terminal;
    (void)nx_request_transition(&uart->active->base, NX_REQUEST_DRAINING);
    if (nx_deadline_finite_after(nx_time_now_us(),
                                 20000000U / uart->baud + 100U,
                                 &uart->drain_deadline) != NX_SUCCESS) {
        /* Exhaustion cannot grant DMA or the wire an infinite drain grace. */
        uart->drain_deadline = 0U;
    }
    bool untouched = (resources->transmit.stream->CR & DMA_SxCR_EN) == 0U &&
                     resources->transmit.stream->NDTR == uart->active->length;
    uart->tx_complete =
        untouched || LL_USART_IsActiveFlag_TC(uart->registers) != 0U;
    if (!uart->tx_complete) {
        uart->registers->CR1 |= USART_CR1_TCIE;
    }
}

/** \brief Cancel cannot revoke the request borrow. */
nx_result_t nx_stm32_uart_dma_cancel_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (uart->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(saved);
        return result;
    }
    if (!uart->tx_complete && !transmit->aborting) {
        begin_drain(uart, resources, transmit, NX_ERROR_CANCELLED);
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief DMA completion enables a distinct UART wire completion observation.
 */
void nx_stm32_uart_dma_irq_parts(nx_stm32_uart_state_t* uart,
                                 const nx_stm32_uart_dma_resources_t* resources,
                                 nx_stm32_uart_tx_dma_extension_t* transmit) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        !uart->initialized) {
        return;
    }
    uint32_t status = flags(resources);
    if ((status & 0x2DU) == 0U) {
        return;
    }
    if (uart->active == NULL) {
        acknowledge(resources);
        return;
    }
    if (!transmit->aborting && (status & 0x0DU) != 0U) {
        begin_drain(uart, resources, transmit, NX_ERROR_IO);
        (void)nx_irq_wake_signal(uart->wake);
    } else if (!transmit->aborting && !transmit->dma_complete) {
        if ((status & 0x0DU) != 0U || resources->transmit.stream->NDTR != 0U) {
            begin_drain(uart, resources, transmit, NX_ERROR_IO);
        } else {
            if (nx_deadline_finite_after(nx_time_now_us(),
                                         20000000U / uart->baud + 100U,
                                         &uart->drain_deadline) != NX_SUCCESS) {
                uart->drain_deadline = 0U;
            }
            detach(uart, resources);
            if ((flags(resources) & 0x0DU) != 0U) {
                begin_drain(uart, resources, transmit, NX_ERROR_IO);
            } else {
                transmit->dma_complete = true;
                uart->registers->CR1 |= USART_CR1_TCIE;
            }
        }
        (void)nx_irq_wake_signal(uart->wake);
    } else {
        acknowledge_bits(resources, flags(resources) & 0x30U);
    }
}

/** \brief Settle only after independently proved memory and wire drain. */
void nx_stm32_uart_dma_service_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit) {
    if (uart == NULL || resources == NULL || transmit == NULL ||
        nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_uart_tx_request_t* request = uart->active;
    if (request == NULL) {
        nx_arch_irq_restore(saved);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    if (!transmit->aborting && (flags(resources) & 0x0DU) != 0U) {
        begin_drain(uart, resources, transmit, NX_ERROR_IO);
    }
    if (!uart->tx_complete && !transmit->aborting &&
        nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(uart, resources, transmit, NX_ERROR_TIMEOUT);
    }
    nx_dma_drain_facts_t facts = {
        .length = request->length,
        .remaining = resources->transmit.stream->NDTR,
        .engine_disabled = (resources->transmit.stream->CR & DMA_SxCR_EN) == 0U,
        .irq_detached = (resources->transmit.stream->CR &
                         (DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
                          DMA_SxCR_DMEIE)) == 0U,
        .peripheral_idle = uart->tx_complete,
    };
    if (nx_dma_drain_check(&facts) == NX_SUCCESS) {
        uart->registers->CR1 &= ~(uint32_t)(USART_CR1_TCIE | USART_CR1_TXEIE);
        acknowledge_bits(resources, flags(resources) & 0x30U);
        /* Actual EN/IRQ withdrawal precedes this final error observation. */
        if (uart->terminal == NX_SUCCESS && (flags(resources) & 0x0DU) != 0U) {
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
        if (!transmit->aborting && transmit->dma_complete &&
            nx_deadline_expired(uart->drain_deadline, now)) {
            nx_time_us_t deadline = uart->drain_deadline;
            begin_drain(uart, resources, transmit, NX_ERROR_IO);
            uart->drain_deadline = deadline;
        }
        if (transmit->aborting &&
            nx_deadline_expired(uart->drain_deadline, now) &&
            nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
    nx_arch_irq_restore(saved);
}

/** \brief Quiesce TX extension without releasing its shared UART owner. */
nx_result_t
nx_stm32_uart_dma_stop_parts(nx_stm32_uart_state_t* uart,
                             const nx_stm32_uart_dma_resources_t* resources,
                             nx_stm32_uart_tx_dma_extension_t* transmit) {
    if (uart == NULL || resources == NULL || transmit == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!uart->initialized) {
        return NX_SUCCESS;
    }
    uart->closing = true;
    if (uart->active != NULL) {
        (void)nx_stm32_uart_dma_cancel_parts(uart, resources, transmit,
                                             uart->active);
        nx_stm32_uart_dma_service_parts(uart, resources, transmit);
        if (uart->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    detach(uart, resources);
    if ((resources->transmit.stream->CR & DMA_SxCR_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(resources->transmit.irq);
    NVIC_ClearPendingIRQ(resources->transmit.irq);
#endif
    return NX_SUCCESS;
}

/** \brief Direct request admission over explicit selected components. */
nx_result_t nx_stm32_uart_dma_submit_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request) {
    return start(uart, resources, transmit, request, false);
}

/** \brief Transfer an adapter loan without readmission. */
nx_result_t nx_stm32_uart_dma_start_admitted_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit, nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(uart, resources, transmit, request, true);
}

/** \brief Expose the matching pure selected validator. */
nx_result_t nx_stm32_uart_dma_tx_validate_parts(
    nx_stm32_uart_state_t* uart, const nx_stm32_uart_dma_resources_t* resources,
    nx_stm32_uart_tx_dma_extension_t* transmit,
    const nx_uart_tx_request_t* request) {
    return validate_tx(uart, resources, transmit, request);
}

/** \brief Decode one finite TX owner; storage remains allocation-free. */
nx_result_t nx_stm32_uart_dma_initialize(nx_stm32_uart_dma_state_t* state,
                                         uint32_t clock_hz) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32_uart_dma_initialize_parts(
        &state->uart, state->resources, &state->transmit, clock_hz, false);
}

/** \brief Decode one finite TX owner; storage remains allocation-free. */
nx_result_t nx_stm32_uart_dma_block_initialize(nx_stm32_uart_dma_state_t* state,
                                               uint32_t clock_hz) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32_uart_dma_initialize_parts(&state->uart, state->resources,
                                              &state->transmit, clock_hz, true);
}

/** \brief Decode the immutable face context into explicit typed components. */
nx_result_t nx_stm32_uart_dma_submit(void* context,
                                     nx_uart_tx_request_t* request) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32_uart_dma_submit_parts(&state->uart, state->resources,
                                          &state->transmit, request);
}

/** \brief Decode the immutable face context into explicit typed components. */
nx_result_t nx_stm32_uart_dma_start_admitted(void* context,
                                             nx_uart_tx_request_t* request) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32_uart_dma_start_admitted_parts(
        &state->uart, state->resources, &state->transmit, request);
}

/** \brief Decode the immutable face context into explicit typed components. */
nx_result_t nx_stm32_uart_dma_cancel(void* context,
                                     nx_uart_tx_request_t* request) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32_uart_dma_cancel_parts(&state->uart, state->resources,
                                          &state->transmit, request);
}

/** \brief Use the one UART identity shared with its selected TX extension. */
void nx_stm32_uart_dma_service(void* context) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return;
    }
    nx_stm32_uart_dma_service_parts(&state->uart, state->resources,
                                    &state->transmit);
}

/** \brief Use the one UART identity shared with its selected TX extension. */
void nx_stm32_uart_dma_irq(nx_stm32_uart_dma_state_t* state) {
    if (state == NULL) {
        return;
    }
    nx_stm32_uart_dma_irq_parts(&state->uart, state->resources,
                                &state->transmit);
}

/** \brief Release the UART only after its finite TX extension is quiescent. */
nx_result_t nx_stm32_uart_dma_stop(void* context) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_stm32_uart_dma_stop_parts(
        &state->uart, state->resources, &state->transmit);
    return result == NX_SUCCESS ? nx_stm32_uart_stop(&state->uart) : result;
}

/** \brief Finite TX mode retains independent IRQ RX. */
void nx_stm32_uart_dma_uart_irq(nx_stm32_uart_dma_state_t* state) {
    if (state != NULL) {
        nx_stm32_uart_irq(&state->uart);
    }
}

/** \brief Preserve independently budgeted byte RX and wake semantics. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!state->uart.initialized || state->uart.closing)
        return NX_ERROR_STATE;
    nx_result_t result = nx_stm32_irq_wake_validate(
        wake, state->resources->transmit.irq, syscall_ceiling);
    return result == NX_SUCCESS ? nx_stm32_uart_ops.attach_wake(
                                      &state->uart, wake, syscall_ceiling)
                                : result;
}

/** \brief Preserve independently budgeted byte RX and wake semantics. */
static nx_result_t read_events(void* context, nx_uart_rx_event_t* events,
                               size_t capacity, size_t* count) {
    nx_stm32_uart_dma_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_uart_read_events(&state->uart, events,
                                                     capacity, count);
}

/** \brief Preserve independently budgeted byte RX and wake semantics. */
static nx_result_t read_bytes(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    nx_stm32_uart_dma_state_t* state = context;
    return state == NULL
               ? NX_ERROR_INVALID
               : nx_stm32_uart_read_bytes(&state->uart, bytes, capacity, count);
}

/** \brief One shared immutable finite TX DMA operations table. */
const nx_uart_ops_t nx_stm32_uart_dma_ops = {
    .submit = nx_stm32_uart_dma_submit,
    .start_admitted = nx_stm32_uart_dma_start_admitted,
    .cancel = nx_stm32_uart_dma_cancel,
    .service = nx_stm32_uart_dma_service,
    .read_events = read_events,
    .read_bytes = read_bytes,
    .stop = nx_stm32_uart_dma_stop,
    .attach_wake = attach_wake,
};

/** \brief Match the actual finite TX face admission checks. */
nx_result_t nx_stm32_uart_dma_tx_validate(void* context,
                                          const nx_uart_tx_request_t* request) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_stm32_uart_dma_tx_validate_parts(&state->uart, state->resources,
                                               &state->transmit, request);
}
