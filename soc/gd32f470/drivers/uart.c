/**
 * \file            uart.c
 * \brief           GD32 USART0 caller-owned TX, true TC and bounded RX storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"
#include <string.h>

const nx_gd32_uart_controller_t nx_gd32_usart0_controller = {
    USART0, 100000000U, RCU_USART0, RCU_USART0RST, USART0_IRQn};
const nx_gd32_uart_controller_t nx_gd32_usart1_controller = {
    USART1, 50000000U, RCU_USART1, RCU_USART1RST, USART1_IRQn};

/** \brief           Configure fixed 8N1 without starting a TX request. */
static void configure(nx_gd32_uart_state_t* port, uint32_t baud) {
    USART_BAUD(port->controller->registers) =
        (port->controller->clock_hz + baud / 2U) / baud;
    USART_CTL0(port->controller->registers) =
        USART_CTL0_UEN | USART_CTL0_TEN | USART_CTL0_REN;
    USART_CTL1(port->controller->registers) = 0u;
    USART_CTL2(port->controller->registers) = 0u;
    if (port->profile != NX_UART_RX_BLOCKS) {
        USART_CTL0(port->controller->registers) |=
            USART_CTL0_RBNEIE | USART_CTL0_PERRIE;
        USART_CTL2(port->controller->registers) = USART_CTL2_ERRIE;
    }
}

/** \brief           Record bounded loss without replacing already queued data.
 */
static void enqueue(nx_gd32_uart_state_t* port, nx_uart_rx_event_t event) {
    if (port->profile == NX_UART_RX_BLOCKS) {
        return;
    }
    if (port->rx_count == port->rx_capacity || port->losses != 0u) {
        if (!port->losses) {
            port->loss_timestamp = event.timestamp_us;
        }
        if (port->losses != UINT32_MAX) {
            ++port->losses;
        }
        return;
    }
    size_t tail = port->rx_head + port->rx_count;
    if (tail >= port->rx_capacity) {
        tail -= port->rx_capacity;
    }
    if (port->profile == NX_UART_RX_EVENTS) {
        ((nx_uart_rx_event_t*)port->rx_storage)[tail] = event;
    } else if (!event.flags) {
        ((uint8_t*)port->rx_storage)[tail] = event.byte;
    } else {
        ++port->losses;
        return;
    }
    ++port->rx_count;
}

/** \brief Initialize one statically selected controller after Board
 * preparation. */
nx_result_t
nx_gd32_uart_initialize_at(nx_gd32_uart_state_t* port,
                           const nx_gd32_uart_controller_t* controller,
                           uint32_t baud, nx_uart_rx_profile_t profile,
                           void* storage, size_t capacity, unsigned priority) {
    if (port == NULL ||
        (controller != &nx_gd32_usart0_controller &&
         controller != &nx_gd32_usart1_controller) ||
        baud == 0U || baud > 1000000U || capacity > SIZE_MAX / 2U ||
        priority > 15U ||
        (profile != NX_UART_RX_BYTES && profile != NX_UART_RX_EVENTS &&
         profile != NX_UART_RX_BLOCKS) ||
        (profile == NX_UART_RX_BLOCKS ? (storage != NULL || capacity != 0U)
                                      : (storage == NULL || capacity == 0U)) ||
        (profile == NX_UART_RX_EVENTS &&
         (capacity > SIZE_MAX / sizeof(nx_uart_rx_event_t) ||
          (uintptr_t)storage % _Alignof(nx_uart_rx_event_t) != 0U))) {
        return NX_ERROR_INVALID;
    }
    uint32_t divisor = (controller->clock_hz + baud / 2U) / baud;
    if (divisor == 0U || divisor > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_gd32_uart_state_t){.controller = controller,
                                   .rx_storage = storage,
                                   .rx_capacity = capacity,
                                   .baud = baud,
                                   .profile = profile};
    rcu_periph_clock_enable((rcu_periph_enum)controller->clock);
    usart_deinit(controller->registers);
    configure(port, baud);
    NVIC_ClearPendingIRQ((IRQn_Type)controller->irq);
    NVIC_SetPriority((IRQn_Type)controller->irq, priority);
    port->initialized = true;
    NVIC_EnableIRQ((IRQn_Type)controller->irq);
    return NX_SUCCESS;
}

/** \brief Preserve the explicit USART0 fixture while generated boards use _at.
 */
nx_result_t nx_gd32_uart_initialize(nx_gd32_uart_state_t* port, uint32_t baud,
                                    nx_uart_rx_profile_t profile, void* storage,
                                    size_t capacity, unsigned priority) {
    nx_result_t result =
        nx_gd32_uart_initialize_at(port, &nx_gd32_usart0_controller, baud,
                                   profile, storage, capacity, priority);
    if (result != NX_SUCCESS) {
        return result;
    }
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_9 | GPIO_PIN_10);
    return NX_SUCCESS;
}

/** \brief           Validate both direct and adapter start without admission.
 */
static nx_result_t validate(nx_gd32_uart_state_t* port,
                            nx_uart_tx_request_t* request) {
    if (!port || port->controller == NULL || !request || !request->data ||
        !request->length) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->initialized || port->stopping) {
        return NX_ERROR_STATE;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return NX_SUCCESS;
}

/** \brief           Start hardware only after the unique admission succeeds. */
static void start(nx_gd32_uart_state_t* port, nx_uart_tx_request_t* request) {
    port->tx_position = 0u;
    port->terminal = NX_SUCCESS;
    port->tc = false;
    port->active = request;
    USART_STAT0(port->controller->registers) &= ~USART_STAT0_TC;
    USART_CTL0(port->controller->registers) |= USART_CTL0_TBEIE;
}

/** \brief           Admit and start one direct caller-owned request. */
nx_result_t nx_gd32_uart_submit(void* context, nx_uart_tx_request_t* request) {
    nx_gd32_uart_state_t* port = context;
    nx_result_t status = validate(port, request);
    if (status != NX_SUCCESS) {
        return status;
    }
    uint32_t saved = nx_gd32_critical_enter();
    status = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (status == NX_SUCCESS) {
        start(port, request);
    }
    nx_gd32_critical_leave(saved);
    return status;
}

/** \brief           Transfer an existing adapter admission to the hardware. */
nx_result_t nx_gd32_uart_start_admitted(void* context,
                                        nx_uart_tx_request_t* request) {
    nx_gd32_uart_state_t* port = context;
    nx_result_t status = validate(port, request);
    if (status != NX_SUCCESS) {
        return status;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        status = NX_ERROR_STATE;
    } else {
        status = nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
        if (status == NX_SUCCESS) {
            start(port, request);
        }
    }
    nx_gd32_critical_leave(saved);
    return status;
}

/** \brief           Retain the borrow until task service proves drain. */
nx_result_t nx_gd32_uart_cancel(void* context, nx_uart_tx_request_t* request) {
    nx_gd32_uart_state_t* port = context;
    if (!port || !request || port->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_result_t result = NX_SUCCESS;
    if (nx_request_state(&request->base) != NX_REQUEST_SETTLED) {
        if (port->active != request) {
            result = NX_ERROR_STATE;
        } else if (!port->tc && port->terminal == NX_SUCCESS) {
            port->terminal = NX_ERROR_CANCELLED;
        }
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief           Publish settlement only after detaching all hardware refs.
 */
void nx_gd32_uart_service(void* context) {
    nx_gd32_uart_state_t* port = context;
    if (!port || port->controller == NULL || nx_gd32_in_isr() ||
        nx_gd32_irq_masked()) {
        return;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_uart_tx_request_t* request = port->active;
    if (!request) {
        nx_gd32_critical_leave(saved);
        return;
    }
    if (!port->tc && port->terminal == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        port->terminal = NX_ERROR_TIMEOUT;
    }
    nx_result_t result = port->terminal;
    bool aborting = result != NX_SUCCESS;
    if (!port->tc && !aborting) {
        nx_gd32_critical_leave(saved);
        return;
    }
    USART_CTL0(port->controller->registers) &=
        ~(USART_CTL0_TBEIE | USART_CTL0_TCIE);
    if (aborting && !port->tc) {
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
        USART_CTL0(port->controller->registers) = 0u;
        rcu_periph_reset_enable((rcu_periph_reset_enum)port->controller->reset);
        rcu_periph_reset_disable(
            (rcu_periph_reset_enum)port->controller->reset);
        nx_gd32_peripheral_barrier();
        if (USART_CTL0(port->controller->registers) != 0u) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
            nx_gd32_critical_leave(saved);
            return;
        }
        NVIC_ClearPendingIRQ((IRQn_Type)port->controller->irq);
        configure(port, port->baud);
        enqueue(port, (nx_uart_rx_event_t){.timestamp_us = nx_time_now_us(),
                                           .flags = NX_UART_EVENT_LOSS |
                                                    NX_UART_EVENT_NO_BYTE});
    }
    size_t transferred = port->tc ? port->tx_position : 0u;
    port->active = NULL;
    nx_gd32_peripheral_barrier();
    nx_request_settle(&request->base, result, transferred);
    nx_gd32_critical_leave(saved);
}

/** \brief           Copy queued event facts and preserve observable overflow.
 */
nx_result_t nx_gd32_uart_read_events(void* context, nx_uart_rx_event_t* events,
                                     size_t capacity, size_t* count) {
    nx_gd32_uart_state_t* port = context;
    if (!port || port->controller == NULL || !events || !capacity || !count) {
        return NX_ERROR_INVALID;
    }
    if (port->profile != NX_UART_RX_EVENTS) {
        return NX_ERROR_UNSUPPORTED;
    }
    *count = 0u;
    while (*count < capacity) {
        uint32_t saved = nx_gd32_critical_enter();
        if (port->rx_count) {
            events[(*count)++] =
                ((nx_uart_rx_event_t*)port->rx_storage)[port->rx_head];
            if (++port->rx_head == port->rx_capacity) {
                port->rx_head = 0u;
            }
            --port->rx_count;
        } else if (port->losses) {
            events[(*count)++] = (nx_uart_rx_event_t){
                .timestamp_us = port->loss_timestamp,
                .flags = NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE};
            port->losses = 0u;
        } else {
            nx_gd32_critical_leave(saved);
            break;
        }
        nx_gd32_critical_leave(saved);
    }
    return *count ? NX_SUCCESS : NX_ERROR_EMPTY;
}

/** \brief           Consume byte-profile data with aggregate loss status. */
nx_result_t nx_gd32_uart_read_bytes(void* context, uint8_t* bytes,
                                    size_t capacity, size_t* count) {
    nx_gd32_uart_state_t* port = context;
    if (!port || port->controller == NULL || !bytes || !capacity || !count) {
        return NX_ERROR_INVALID;
    }
    if (port->profile != NX_UART_RX_BYTES) {
        return NX_ERROR_UNSUPPORTED;
    }
    *count = 0u;
    bool overflow = false;
    while (*count < capacity) {
        uint32_t saved = nx_gd32_critical_enter();
        overflow |= port->losses != 0u;
        if (!port->rx_count) {
            port->losses = 0u;
            nx_gd32_critical_leave(saved);
            break;
        }
        bytes[(*count)++] = ((uint8_t*)port->rx_storage)[port->rx_head];
        if (++port->rx_head == port->rx_capacity) {
            port->rx_head = 0u;
        }
        --port->rx_count;
        if (!port->rx_count) {
            port->losses = 0u;
        }
        nx_gd32_critical_leave(saved);
    }
    return overflow ? NX_ERROR_OVERFLOW : *count ? NX_SUCCESS : NX_ERROR_EMPTY;
}

/** \brief           Close admissions, retaining storage until TX drain ends. */
nx_result_t nx_gd32_uart_stop(void* context) {
    nx_gd32_uart_state_t* port = context;
    if (!port || port->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    port->stopping = true;
    if (port->active) {
        if (!port->tc && port->terminal == NX_SUCCESS) {
            port->terminal = NX_ERROR_CANCELLED;
        }
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    NVIC_DisableIRQ((IRQn_Type)port->controller->irq);
    USART_CTL0(port->controller->registers) = 0u;
    USART_CTL2(port->controller->registers) = 0u;
    usart_deinit(port->controller->registers);
    NVIC_ClearPendingIRQ((IRQn_Type)port->controller->irq);
    nx_gd32_peripheral_barrier();
    port->wake = NULL;
    port->initialized = false;
    rcu_periph_clock_disable((rcu_periph_enum)port->controller->clock);
    port->controller = NULL;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief Share true wire-completion handling without touching RX DATA. */
bool nx_gd32_uart_tx_irq(nx_gd32_uart_state_t* port, uint32_t flags) {
    if (!port || !port->initialized) {
        return false;
    }
    bool sent_byte = false;
    bool notify = false;
    if (port->active && (flags & USART_STAT0_TBE) != 0u &&
        (USART_CTL0(port->controller->registers) & USART_CTL0_TBEIE) != 0u) {
        USART_DATA(port->controller->registers) =
            port->active->data[port->tx_position++];
        sent_byte = true;
        if (port->tx_position == port->active->length) {
            USART_CTL0(port->controller->registers) =
                (USART_CTL0(port->controller->registers) & ~USART_CTL0_TBEIE) |
                USART_CTL0_TCIE;
        }
    }
    if (port->active && (flags & USART_STAT0_TC) != 0u &&
        (USART_CTL0(port->controller->registers) & USART_CTL0_TCIE) != 0u &&
        port->tx_position == port->active->length && !sent_byte) {
        USART_CTL0(port->controller->registers) &= ~USART_CTL0_TCIE;
        if (port->terminal == NX_SUCCESS &&
            nx_deadline_expired(port->active->base.deadline,
                                nx_time_now_us())) {
            port->terminal = NX_ERROR_TIMEOUT;
        }
        port->tc = true;
        notify = true;
    }
    return notify;
}

/** \brief Move bounded bytes and publish TC facts without consumer callbacks.
 */
void nx_gd32_uart_irq(nx_gd32_uart_state_t* port) {
    if (port == NULL || !port->initialized) {
        return;
    }
    bool notify = false;
    uint32_t flags = USART_STAT0(port->controller->registers);
    uint32_t errors = flags & (USART_STAT0_PERR | USART_STAT0_FERR |
                               USART_STAT0_NERR | USART_STAT0_ORERR);
    if ((flags & USART_STAT0_RBNE) != 0U || errors != 0U) {
        notify = true;
        uint8_t byte = (uint8_t)USART_DATA(port->controller->registers);
        uint32_t neutral =
            (errors & USART_STAT0_PERR ? NX_UART_EVENT_PARITY : 0U) |
            (errors & USART_STAT0_FERR ? NX_UART_EVENT_FRAMING : 0U) |
            (errors & USART_STAT0_NERR ? NX_UART_EVENT_NOISE : 0U) |
            (errors & USART_STAT0_ORERR ? NX_UART_EVENT_OVERRUN : 0U) |
            ((flags & USART_STAT0_RBNE) == 0U ? NX_UART_EVENT_NO_BYTE : 0U);
        enqueue(port, (nx_uart_rx_event_t){nx_time_now_us(), neutral, byte});
    }
    notify = nx_gd32_uart_tx_irq(port, flags) || notify;
    if (notify) {
        (void)nx_irq_wake_signal(port->wake);
    }
}

/** \brief Attach only after checking the actual publisher priority. */
static nx_result_t gd32_uart_attach_wake(void* context,
                                         const nx_irq_wake_t* wake,
                                         uint8_t syscall_ceiling) {
    nx_gd32_uart_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (!port->initialized) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_STATE;
    }
    nx_result_t result =
        nx_gd32_irq_wake_validate(wake, port->controller->irq, syscall_ceiling);
    if (result == NX_SUCCESS) {
        port->wake = wake;
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_uart_ops_t nx_gd32_uart_ops = {
    .submit = nx_gd32_uart_submit,
    .start_admitted = nx_gd32_uart_start_admitted,
    .cancel = nx_gd32_uart_cancel,
    .service = nx_gd32_uart_service,
    .read_events = nx_gd32_uart_read_events,
    .read_bytes = nx_gd32_uart_read_bytes,
    .stop = nx_gd32_uart_stop,
    .attach_wake = gd32_uart_attach_wake,
};
