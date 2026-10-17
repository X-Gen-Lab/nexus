/**
 * \file            uart.c
 * \brief           GD32 USART0 caller-owned TX, true TC and bounded RX storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../../common/uart_control.h"
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"
#include <string.h>

const nx_gd32_uart_controller_t nx_gd32_usart0_controller = {
    USART0, 100000000U, RCU_USART0, RCU_USART0RST, USART0_IRQn};
const nx_gd32_uart_controller_t nx_gd32_usart1_controller = {
    USART1, 50000000U, RCU_USART1, RCU_USART1RST, USART1_IRQn};

#ifndef NX_GD32_UART_ENABLE
#define NX_GD32_UART_ENABLE(registers, control)                                \
    NX_UART_REG_WRITE(&USART_CTL0(registers), (uint32_t)(control))
#endif

/** \brief The selected format is complete before UEN permits reception. */
static void configure_frame(nx_gd32_uart_state_t* port, uint32_t baud,
                            uint32_t frame, uint32_t stop, uint32_t flow) {
    uint32_t uart = port->controller->registers;
    NX_UART_REG_WRITE(&USART_CTL0(uart), 0U);
    NX_UART_REG_WRITE(&USART_BAUD(uart),
                      (port->controller->clock_hz + baud / 2U) / baud);
    NX_UART_REG_WRITE(&USART_CTL1(uart), stop);
    NX_UART_REG_WRITE(&USART_CTL2(uart), flow);
    uint32_t control = USART_CTL0_UEN | USART_CTL0_TEN | USART_CTL0_REN | frame;
    if (port->profile != NX_UART_RX_BLOCKS) {
        control |= USART_CTL0_RBNEIE | USART_CTL0_PERRIE;
        NX_UART_REG_WRITE(&USART_CTL2(uart),
                          USART_CTL2(uart) | (USART_CTL2_ERRIE));
    }
    NX_GD32_UART_ENABLE(uart, control);
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
static inline nx_result_t initialize_frame_at(
    nx_gd32_uart_state_t* port, const nx_gd32_uart_controller_t* controller,
    uint32_t baud, nx_uart_rx_profile_t profile, void* storage, size_t capacity,
    unsigned priority, uint32_t frame, uint32_t stop, uint32_t flow) {
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
    uint32_t divisor =
        controller->clock_hz / baud +
        (controller->clock_hz % baud >= baud / 2U + baud % 2U ? 1U : 0U);
    if (divisor == 0U || divisor > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->initialized || port->active != NULL ||
        !NX_UART_IRQ_QUIET(controller->irq) ||
        (RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_gd32_uart_state_t){.controller = controller,
                                   .rx_storage = storage,
                                   .rx_capacity = capacity,
                                   .baud = baud,
                                   .profile = profile,
                                   .initialized = true,
                                   .stopping = true};
    NX_UART_CLOCK_ON(controller->clock);
    nx_gd32_peripheral_barrier();
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) == 0U) {
        goto failed;
    }
    usart_deinit(controller->registers);
    NX_UART_REG_WRITE(&USART_CTL0(controller->registers), 0U);
    nx_gd32_peripheral_barrier();
    if (USART_CTL0(controller->registers) != 0U) {
        goto failed;
    }
    configure_frame(port, baud, frame, stop, flow);
    nx_gd32_peripheral_barrier();
    uint32_t expected0 =
        USART_CTL0_UEN | USART_CTL0_TEN | USART_CTL0_REN | frame |
        (profile == NX_UART_RX_BLOCKS
             ? 0U
             : (uint32_t)(USART_CTL0_RBNEIE | USART_CTL0_PERRIE));
    uint32_t expected2 =
        flow | (profile == NX_UART_RX_BLOCKS ? 0U : (uint32_t)USART_CTL2_ERRIE);
    if (USART_CTL0(controller->registers) != expected0 ||
        USART_CTL1(controller->registers) != stop ||
        USART_CTL2(controller->registers) != expected2 ||
        USART_BAUD(controller->registers) != divisor) {
        goto failed;
    }
    NX_UART_IRQ_CLEAR_PENDING(controller->irq);
    NX_UART_IRQ_SET_PRIORITY(controller->irq, priority);
    NX_UART_IRQ_ENABLE(controller->irq);
    nx_gd32_peripheral_barrier();
    if (!NX_UART_IRQ_CONFIGURED(controller->irq, priority) ||
        NX_UART_IRQ_ACTIVE(controller->irq) != 0U) {
        goto failed;
    }
    port->stopping = false;
    return NX_SUCCESS;
failed:
    (void)nx_gd32_uart_stop(port);
    return NX_ERROR_IO;
}

/** \brief Preserve exact default-frame assembly without added state. */
nx_result_t
nx_gd32_uart_initialize_at(nx_gd32_uart_state_t* port,
                           const nx_gd32_uart_controller_t* controller,
                           uint32_t baud, nx_uart_rx_profile_t profile,
                           void* storage, size_t capacity, unsigned priority) {
    return initialize_frame_at(port, controller, baud, profile, storage,
                               capacity, priority, 0U, 0U, 0U);
}
/** \brief Program parity, stop and flow before any UEN/IRQ enable. */
nx_result_t nx_gd32_uart_initialize_format_at(
    nx_gd32_uart_state_t* port, const nx_gd32_uart_controller_t* controller,
    uint32_t baud, nx_uart_rx_profile_t profile, void* storage, size_t capacity,
    unsigned priority, nx_uart_format_t format) {
    nx_result_t result = nx_uart_format_validate(format);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (format.data_bits == 9U || profile == NX_UART_RX_BLOCKS) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t frame =
        (format.data_bits == 8U && format.parity != NX_UART_PARITY_NONE
             ? USART_CTL0_WL
             : 0U) |
        (format.parity != NX_UART_PARITY_NONE ? USART_CTL0_PCEN : 0U) |
        (format.parity == NX_UART_PARITY_ODD ? USART_CTL0_PM : 0U);
    uint32_t stop = format.stop == NX_UART_STOP_2 ? 2U << 12U : 0U;
    uint32_t flow = ((format.flow == NX_UART_FLOW_RTS ||
                      format.flow == NX_UART_FLOW_RTS_CTS)
                         ? USART_CTL2_RTSEN
                         : 0U) |
                    ((format.flow == NX_UART_FLOW_CTS ||
                      format.flow == NX_UART_FLOW_RTS_CTS)
                         ? USART_CTL2_CTSEN
                         : 0U);
    return initialize_frame_at(port, controller, baud, profile, storage,
                               capacity, priority, frame, stop, flow);
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
    NX_UART_CLOCK_ON(RCU_GPIOA);
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_9 | GPIO_PIN_10);
    return NX_SUCCESS;
}

/** \brief           Validate both direct and adapter start without admission.
 */
static inline nx_result_t validate(const nx_gd32_uart_state_t* port,
                                   const nx_uart_tx_request_t* request) {
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
    NX_UART_REG_WRITE(&USART_CTL0(port->controller->registers),
                      USART_CTL0(port->controller->registers) |
                          (USART_CTL0_TBEIE));
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
    NX_UART_REG_WRITE(&USART_CTL0(port->controller->registers),
                      USART_CTL0(port->controller->registers) &
                          (~(USART_CTL0_TBEIE | USART_CTL0_TCIE)));
    if (aborting && !port->tc) {
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
        uint32_t flow = USART_CTL2(port->controller->registers) &
                        (USART_CTL2_RTSEN | USART_CTL2_CTSEN);
        uint32_t frame = USART_CTL0(port->controller->registers) &
                         (USART_CTL0_WL | USART_CTL0_PCEN | USART_CTL0_PM);
        uint32_t stop =
            USART_CTL1(port->controller->registers) & USART_CTL1_STB;
        NX_UART_REG_WRITE(&USART_CTL0(port->controller->registers), 0u);
        rcu_periph_reset_enable((rcu_periph_reset_enum)port->controller->reset);
        rcu_periph_reset_disable(
            (rcu_periph_reset_enum)port->controller->reset);
        nx_gd32_peripheral_barrier();
        if (USART_CTL0(port->controller->registers) != 0u) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
            nx_gd32_critical_leave(saved);
            return;
        }
        NX_UART_IRQ_CLEAR_PENDING((IRQn_Type)port->controller->irq);
        configure_frame(port, port->baud, frame, stop, flow);
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
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->initialized) {
        return NX_SUCCESS;
    }
    if (port->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    uint32_t saved = nx_gd32_critical_enter();
    port->stopping = true;
    if (port->active != NULL) {
        if (!port->tc && port->terminal == NX_SUCCESS) {
            port->terminal = NX_ERROR_CANCELLED;
        }
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    const nx_gd32_uart_controller_t* controller = port->controller;
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        NX_UART_REG_WRITE(&USART_CTL0(controller->registers), 0U);
        NX_UART_REG_WRITE(&USART_CTL2(controller->registers), 0U);
        nx_gd32_peripheral_barrier();
        if (USART_CTL0(controller->registers) != 0U ||
            USART_CTL2(controller->registers) != 0U) {
            nx_gd32_critical_leave(saved);
            return NX_ERROR_BUSY;
        }
    }
    NX_UART_IRQ_DISABLE(controller->irq);
    NX_UART_IRQ_CLEAR_PENDING(controller->irq);
    nx_gd32_peripheral_barrier();
    if (!NX_UART_IRQ_QUIET(controller->irq)) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    NX_UART_CLOCK_OFF(controller->clock);
    nx_gd32_peripheral_barrier();
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    port->wake = NULL;
    port->initialized = false;
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
    if (port->active && port->tx_position < port->active->length &&
        (flags & USART_STAT0_TBE) != 0u &&
        (USART_CTL0(port->controller->registers) & USART_CTL0_TBEIE) != 0u) {
        USART_DATA(port->controller->registers) =
            port->active->data[port->tx_position++];
        sent_byte = true;
        if (port->tx_position == port->active->length) {
            NX_UART_REG_WRITE(
                &USART_CTL0(port->controller->registers),
                (USART_CTL0(port->controller->registers) & ~USART_CTL0_TBEIE) |
                    USART_CTL0_TCIE);
        }
    }
    if (port->active && (flags & USART_STAT0_TC) != 0u &&
        (USART_CTL0(port->controller->registers) & USART_CTL0_TCIE) != 0u &&
        port->tx_position == port->active->length && !sent_byte) {
        NX_UART_REG_WRITE(&USART_CTL0(port->controller->registers),
                          USART_CTL0(port->controller->registers) &
                              (~USART_CTL0_TCIE));
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
static inline void uart_irq_mask(nx_gd32_uart_state_t* port,
                                 uint8_t payload_mask) {
    if (port == NULL || !port->initialized) {
        return;
    }
    bool notify = false;
    uint32_t flags = USART_STAT0(port->controller->registers);
    uint32_t errors = flags & (USART_STAT0_PERR | USART_STAT0_FERR |
                               USART_STAT0_NERR | USART_STAT0_ORERR);
    if ((flags & USART_STAT0_RBNE) != 0U || errors != 0U) {
        notify = true;
        uint8_t byte =
            (uint8_t)USART_DATA(port->controller->registers) & payload_mask;
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

/** \brief Default byte IRQ retains a constant mask and original state layout.
 */
void nx_gd32_uart_irq(nx_gd32_uart_state_t* port) {
    uart_irq_mask(port, UINT8_MAX);
}
/** \brief Seven-bit parity strips the bit excluded by the public payload width.
 */
void nx_gd32_uart_format_irq(nx_gd32_uart_state_t* port) {
    if (port == NULL || port->controller == NULL) {
        return;
    }
    uint32_t frame = USART_CTL0(port->controller->registers);
    uint8_t mask =
        (frame & (USART_CTL0_WL | USART_CTL0_PCEN)) == USART_CTL0_PCEN
            ? 0x7FU
            : UINT8_MAX;
    uart_irq_mask(port, mask);
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

/** \brief Selected cold hardware flow control preserves unrelated bits. */
nx_result_t nx_gd32_uart_flow_configure(nx_gd32_uart_state_t* state, bool rts,
                                        bool cts) {
    if (state == NULL || state->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (state->active != NULL ||
        (USART_CTL2(state->controller->registers) & USART_CTL2_DENR) != 0U) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    uint32_t original = USART_CTL2(state->controller->registers);
    uint32_t selected =
        (original & ~(uint32_t)(USART_CTL2_RTSEN | USART_CTL2_CTSEN)) |
        (rts ? USART_CTL2_RTSEN : 0U) | (cts ? USART_CTL2_CTSEN : 0U);
    NX_UART_REG_WRITE(&USART_CTL2(state->controller->registers), selected);
    nx_gd32_peripheral_barrier();
    nx_result_t result = USART_CTL2(state->controller->registers) == selected
                             ? NX_SUCCESS
                             : NX_ERROR_IO;
    if (result != NX_SUCCESS) {
        NX_UART_REG_WRITE(&USART_CTL2(state->controller->registers), original);
        nx_gd32_peripheral_barrier();
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Expose exact pure checks only for an explicitly selected adapter. */
nx_result_t nx_gd32_uart_tx_validate(void* context,
                                     const nx_uart_tx_request_t* request) {
    return validate(context, request);
}

/** \brief Cold format selection keeps default state and hot operations intact.
 */
nx_result_t nx_gd32_uart_format_configure(nx_gd32_uart_state_t* state,
                                          nx_uart_format_t format) {
    nx_result_t result = nx_uart_format_validate(format);
    if (state == NULL || state->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    if (format.data_bits == 9U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->active != NULL || state->rx_count != 0U ||
        (USART_CTL2(state->controller->registers) &
         (USART_CTL2_DENR | USART_CTL2_DENT)) != 0U ||
        (USART_STAT0(state->controller->registers) & USART_STAT0_RBNE) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    uint32_t old1 = USART_CTL0(state->controller->registers);
    uint32_t old2 = USART_CTL1(state->controller->registers);
    uint32_t old3 = USART_CTL2(state->controller->registers);
    uint32_t frame =
        (format.data_bits == 8U && format.parity != NX_UART_PARITY_NONE
             ? USART_CTL0_WL
             : 0U) |
        (format.parity != NX_UART_PARITY_NONE ? USART_CTL0_PCEN : 0U) |
        (format.parity == NX_UART_PARITY_ODD ? USART_CTL0_PM : 0U);
    uint32_t selected1 =
        (old1 & ~(uint32_t)(USART_CTL0_WL | USART_CTL0_PCEN | USART_CTL0_PM)) |
        frame;
    uint32_t selected2 = (old2 & ~(uint32_t)USART_CTL1_STB) |
                         (format.stop == NX_UART_STOP_2 ? 2U << 12U : 0U);
    uint32_t selected3 =
        (old3 & ~(uint32_t)(USART_CTL2_RTSEN | USART_CTL2_CTSEN)) |
        ((format.flow == NX_UART_FLOW_RTS ||
          format.flow == NX_UART_FLOW_RTS_CTS)
             ? USART_CTL2_RTSEN
             : 0U) |
        ((format.flow == NX_UART_FLOW_CTS ||
          format.flow == NX_UART_FLOW_RTS_CTS)
             ? USART_CTL2_CTSEN
             : 0U);
    /* A refused UE withdrawal must not change a live frame. */
    NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                      old1 & ~(uint32_t)USART_CTL0_UEN);
    nx_gd32_peripheral_barrier();
    if ((USART_CTL0(state->controller->registers) & USART_CTL0_UEN) != 0U) {
        if (USART_CTL0(state->controller->registers) != old1) {
            NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers), old1);
            nx_gd32_peripheral_barrier();
            if (USART_CTL0(state->controller->registers) != old1) {
                state->stopping = true;
            }
        }
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    NX_UART_REG_WRITE(&USART_CTL1(state->controller->registers), selected2);
    NX_UART_REG_WRITE(&USART_CTL2(state->controller->registers), selected3);
    NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                      selected1 & ~(uint32_t)USART_CTL0_UEN);
    nx_gd32_peripheral_barrier();
    bool selected = USART_CTL0(state->controller->registers) ==
                        (selected1 & ~(uint32_t)USART_CTL0_UEN) &&
                    USART_CTL1(state->controller->registers) == selected2 &&
                    USART_CTL2(state->controller->registers) == selected3;
    if (selected) {
        NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers), selected1);
        nx_gd32_peripheral_barrier();
        selected = USART_CTL0(state->controller->registers) == selected1 &&
                   USART_CTL1(state->controller->registers) == selected2 &&
                   USART_CTL2(state->controller->registers) == selected3;
    }
    if (selected) {
        result = NX_SUCCESS;
    } else {
        result = NX_ERROR_IO;
        NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                          old1 & ~(uint32_t)USART_CTL0_UEN);
        nx_gd32_peripheral_barrier();
        bool restored =
            (USART_CTL0(state->controller->registers) & USART_CTL0_UEN) == 0U;
        if (restored) {
            NX_UART_REG_WRITE(&USART_CTL1(state->controller->registers), old2);
            NX_UART_REG_WRITE(&USART_CTL2(state->controller->registers), old3);
            NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                              old1 & ~(uint32_t)USART_CTL0_UEN);
            nx_gd32_peripheral_barrier();
            restored = USART_CTL0(state->controller->registers) ==
                           (old1 & ~(uint32_t)USART_CTL0_UEN) &&
                       USART_CTL1(state->controller->registers) == old2 &&
                       USART_CTL2(state->controller->registers) == old3;
        }
        if (restored) {
            NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers), old1);
            nx_gd32_peripheral_barrier();
            restored = USART_CTL0(state->controller->registers) == old1 &&
                       USART_CTL1(state->controller->registers) == old2 &&
                       USART_CTL2(state->controller->registers) == old3;
        }
        if (!restored) {
            /* Keep the initialized lease for explicit stop/recovery. */
            state->stopping = true;
            NX_UART_REG_WRITE(&USART_CTL0(state->controller->registers),
                              old1 & ~(uint32_t)USART_CTL0_UEN);
            nx_gd32_peripheral_barrier();
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}
