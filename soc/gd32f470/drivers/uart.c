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

static nx_uart_port_t* s_uart;

/** \brief           Configure fixed 8N1 without starting a TX request. */
static void configure(uint32_t baud) {
    usart_baudrate_set(USART0, baud);
    USART_CTL0(USART0) = USART_CTL0_UEN | USART_CTL0_TEN | USART_CTL0_REN |
                         USART_CTL0_RBNEIE | USART_CTL0_PERRIE;
    USART_CTL1(USART0) = 0u;
    USART_CTL2(USART0) = USART_CTL2_ERRIE;
}

/** \brief           Record bounded loss without replacing already queued data.
 */
static void enqueue(nx_uart_port_t* port, nx_uart_rx_event_t event) {
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

/** \brief           Assemble exactly one static USART0 handler binding. */
nx_result_t nx_gd32_uart_initialize(nx_uart_port_t* port, uint32_t baud,
                                    nx_uart_rx_profile_t profile, void* storage,
                                    size_t capacity, unsigned priority) {
    if (!port || !storage || !capacity || baud < 1526u || baud > 1000000u ||
        capacity > SIZE_MAX / 2u ||
        (profile == NX_UART_RX_EVENTS &&
         (capacity > SIZE_MAX / sizeof(nx_uart_rx_event_t) ||
          (uintptr_t)storage % _Alignof(nx_uart_rx_event_t) != 0u)) ||
        priority > 15u ||
        (profile != NX_UART_RX_BYTES && profile != NX_UART_RX_EVENTS)) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s_uart) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_uart_port_t){.rx_storage = storage,
                             .rx_capacity = capacity,
                             .baud = baud,
                             .profile = profile};
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_9 | GPIO_PIN_10);
    usart_deinit(USART0);
    configure(baud);
    NVIC_ClearPendingIRQ(USART0_IRQn);
    NVIC_SetPriority(USART0_IRQn, priority);
    s_uart = port;
    port->initialized = true;
    NVIC_EnableIRQ(USART0_IRQn);
    return NX_SUCCESS;
}

/** \brief           Validate both direct and adapter start without admission.
 */
static nx_result_t validate(nx_uart_port_t* port,
                            nx_uart_tx_request_t* request) {
    if (!port || port != s_uart || !request || !request->data ||
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
static void start(nx_uart_port_t* port, nx_uart_tx_request_t* request) {
    port->tx_position = 0u;
    port->terminal = NX_SUCCESS;
    port->tc = false;
    port->active = request;
    USART_STAT0(USART0) &= ~USART_STAT0_TC;
    USART_CTL0(USART0) |= USART_CTL0_TBEIE;
}

/** \brief           Admit and start one direct caller-owned request. */
nx_result_t nx_uart_port_submit(nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
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
nx_result_t nx_uart_port_start_admitted(nx_uart_port_t* port,
                                        nx_uart_tx_request_t* request) {
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
nx_result_t nx_uart_port_cancel(nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
    if (!port || !request || port != s_uart) {
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
void nx_uart_port_service(nx_uart_port_t* port) {
    if (!port || port != s_uart || nx_gd32_in_isr() || nx_gd32_irq_masked()) {
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
    USART_CTL0(USART0) &= ~(USART_CTL0_TBEIE | USART_CTL0_TCIE);
    if (aborting && !port->tc) {
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
        USART_CTL0(USART0) = 0u;
        rcu_periph_reset_enable(RCU_USART0RST);
        rcu_periph_reset_disable(RCU_USART0RST);
        nx_gd32_peripheral_barrier();
        if (USART_CTL0(USART0) != 0u) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
            nx_gd32_critical_leave(saved);
            return;
        }
        NVIC_ClearPendingIRQ(USART0_IRQn);
        configure(port->baud);
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
nx_result_t nx_uart_port_read_events(nx_uart_port_t* port,
                                     nx_uart_rx_event_t* events,
                                     size_t capacity, size_t* count) {
    if (!port || port != s_uart || !events || !capacity || !count) {
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
nx_result_t nx_uart_port_read_bytes(nx_uart_port_t* port, uint8_t* bytes,
                                    size_t capacity, size_t* count) {
    if (!port || port != s_uart || !bytes || !capacity || !count) {
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
nx_result_t nx_uart_port_stop(nx_uart_port_t* port) {
    if (!port || port != s_uart) {
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
    NVIC_DisableIRQ(USART0_IRQn);
    USART_CTL0(USART0) = 0u;
    USART_CTL2(USART0) = 0u;
    usart_deinit(USART0);
    NVIC_ClearPendingIRQ(USART0_IRQn);
    nx_gd32_peripheral_barrier();
    s_uart = NULL;
    port->initialized = false;
    rcu_periph_clock_disable(RCU_USART0);
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief           Move bounded bytes and publish TC facts without callbacks.
 */
void USART0_IRQHandler(void) {
    nx_uart_port_t* port = s_uart;
    if (!port || !port->initialized) {
        return;
    }
    bool sent_byte = false;
    uint32_t flags = USART_STAT0(USART0);
    uint32_t errors = flags & (USART_STAT0_PERR | USART_STAT0_FERR |
                               USART_STAT0_NERR | USART_STAT0_ORERR);
    if ((flags & USART_STAT0_RBNE) != 0u || errors) {
        uint8_t byte = (uint8_t)USART_DATA(USART0);
        uint32_t neutral =
            (errors & USART_STAT0_PERR ? NX_UART_EVENT_PARITY : 0u) |
            (errors & USART_STAT0_FERR ? NX_UART_EVENT_FRAMING : 0u) |
            (errors & USART_STAT0_NERR ? NX_UART_EVENT_NOISE : 0u) |
            (errors & USART_STAT0_ORERR ? NX_UART_EVENT_OVERRUN : 0u) |
            ((flags & USART_STAT0_RBNE) == 0u ? NX_UART_EVENT_NO_BYTE : 0u);
        enqueue(port, (nx_uart_rx_event_t){nx_time_now_us(), neutral, byte});
    }
    if (port->active && (flags & USART_STAT0_TBE) != 0u &&
        (USART_CTL0(USART0) & USART_CTL0_TBEIE) != 0u) {
        USART_DATA(USART0) = port->active->data[port->tx_position++];
        sent_byte = true;
        if (port->tx_position == port->active->length) {
            USART_CTL0(USART0) =
                (USART_CTL0(USART0) & ~USART_CTL0_TBEIE) | USART_CTL0_TCIE;
        }
    }
    if (port->active && (flags & USART_STAT0_TC) != 0u &&
        (USART_CTL0(USART0) & USART_CTL0_TCIE) != 0u &&
        port->tx_position == port->active->length && !sent_byte) {
        USART_CTL0(USART0) &= ~USART_CTL0_TCIE;
        if (port->terminal == NX_SUCCESS &&
            nx_deadline_expired(port->active->base.deadline,
                                nx_time_now_us())) {
            port->terminal = NX_ERROR_TIMEOUT;
        }
        port->tc = true;
    }
}
