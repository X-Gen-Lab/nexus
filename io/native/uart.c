/**
 * \file            uart.c
 *
 * \brief           Deterministic IRQ UART borrowing, TC, RX loss and drain
 *                  model.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/native/model.h"

struct nx_uart_port {
    nx_uart_tx_request_t* active;
    nx_native_uart_config_t config;
    size_t tx_index;
    size_t logged;
    size_t rx_head;
    size_t rx_count;
    bool loss;
    nx_time_us_t loss_timestamp;
    bool terminal;
    bool start_failure;
    bool hold_drain;
    bool opened;
    bool stopping;
    nx_result_t terminal_result;
};
nx_uart_port_t g_nx_native_uart;
nx_uart_port_t* const nx_native_uart = &g_nx_native_uart;

/** \brief Validate fixed-port preparation before establishing any borrow. */
static nx_result_t validate_start(nx_uart_port_t* port,
                                  nx_uart_tx_request_t* request,
                                  nx_request_state_t required) {
    if (port == NULL || request == NULL || request->data == NULL ||
        request->length == 0) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->opened || port->stopping) {
        return NX_ERROR_STATE;
    }
    if (port->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (nx_request_state(&request->base) != required) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return NX_SUCCESS;
}

/** \brief Attach already-admitted storage; later start error stays ACCEPTED. */
static void attach_request(nx_uart_port_t* port,
                           nx_uart_tx_request_t* request) {
    port->active = request;
    port->tx_index = 0;
    port->terminal = port->start_failure;
    port->terminal_result = port->start_failure ? NX_ERROR_IO : NX_SUCCESS;
}

/** \brief Configure only after the previous provider borrow is gone. */
nx_result_t nx_native_uart_configure(const nx_native_uart_config_t* config) {
    if (config == NULL || config->rx_storage == NULL ||
        config->rx_capacity == 0 || config->rx_capacity > SIZE_MAX / 2 ||
        (config->profile != NX_UART_RX_BYTES &&
         config->profile != NX_UART_RX_EVENTS) ||
        (config->profile == NX_UART_RX_EVENTS &&
         (config->rx_capacity > SIZE_MAX / sizeof(nx_uart_rx_event_t) ||
          (uintptr_t)config->rx_storage % _Alignof(nx_uart_rx_event_t) != 0)) ||
        (config->tx_log == NULL && config->tx_capacity != 0)) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    if (g_nx_native_uart.active != NULL) {
        nx_arch_irq_restore(token);
        return NX_ERROR_BUSY;
    }
    g_nx_native_uart = (nx_uart_port_t){0};
    g_nx_native_uart.config = *config;
    g_nx_native_uart.opened = true;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Direct admission occurs once, after every rejectable validation. */
nx_result_t nx_uart_port_submit(nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
    nx_result_t result = validate_start(port, request, NX_REQUEST_READY);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        attach_request(port, request);
    }
    nx_arch_irq_restore(token);
    return result;
}

/** \brief Hand execution from a QUEUED adapter borrow to the controller. */
nx_result_t nx_uart_port_start_admitted(nx_uart_port_t* port,
                                        nx_uart_tx_request_t* request) {
    nx_result_t result = validate_start(port, request, NX_REQUEST_QUEUED);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    result = nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        attach_request(port, request);
    }
    nx_arch_irq_restore(token);
    return result;
}

/** \brief Cancellation retains the descriptor until service proves drain. */
nx_result_t nx_uart_port_cancel(nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
    if (port == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    if (port->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(token);
        return result;
    }
    if (!port->terminal) {
        port->terminal = true;
        port->terminal_result = NX_ERROR_CANCELLED;
    }
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Move at most one byte or one distinct TC fact, never settle in IRQ.
 */
void nx_native_uart_irq_step(void) {
    nx_arch_irq_state_t token = nx_arch_irq_save();
    nx_uart_port_t* port = &g_nx_native_uart;
    if (port->active != NULL && !port->terminal) {
        if (port->tx_index < port->active->length) {
            uint8_t byte = port->active->data[port->tx_index++];
            if (port->config.tx_log != NULL) {
                if (port->logged < port->config.tx_capacity) {
                    port->config.tx_log[port->logged++] = byte;
                } else {
                    port->terminal = true;
                    port->terminal_result = NX_ERROR_OVERFLOW;
                }
            }
        } else {
            port->terminal = true;
            port->terminal_result =
                nx_deadline_expired(port->active->base.deadline,
                                    nx_time_now_us())
                    ? NX_ERROR_TIMEOUT
                    : NX_SUCCESS;
        }
    }
    nx_arch_irq_restore(token);
}

/** \brief Detach before terminal publication and retain failed drain ownership.
 */
void nx_uart_port_service(nx_uart_port_t* port) {
    if (port == NULL || nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t snapshot_token = nx_arch_irq_save();
    if (port->active != NULL && !port->terminal &&
        nx_deadline_expired(port->active->base.deadline, nx_time_now_us())) {
        port->terminal = true;
        port->terminal_result = NX_ERROR_TIMEOUT;
    }
    bool automatic_irq = port->config.automatic_irq;
    nx_arch_irq_restore(snapshot_token);
    if (automatic_irq) {
        nx_native_uart_irq_step();
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    nx_uart_tx_request_t* request = port->active;
    if (request == NULL) {
        nx_arch_irq_restore(token);
        return;
    }
    nx_result_t result = port->terminal_result;
    bool terminal = port->terminal;
    if (terminal && result != NX_SUCCESS) {
        nx_request_state_t state = nx_request_state(&request->base);
        if (state == NX_REQUEST_ACTIVE) {
            (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
        }
    }
    if (!terminal) {
        nx_arch_irq_restore(token);
        return;
    }
    if (port->hold_drain) {
        nx_request_state_t state = nx_request_state(&request->base);
        if (state == NX_REQUEST_ACTIVE || state == NX_REQUEST_DRAINING) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
        nx_arch_irq_restore(token);
        return;
    }
    size_t transferred = port->tx_index;
    port->active = NULL;
    port->terminal = false;
    nx_arch_irq_restore(token);
    nx_request_settle(&request->base, result, transferred);
}

/** \brief Inject one bounded RX event without callbacks or silent overwrite. */
nx_result_t nx_native_uart_receive(uint8_t byte, uint32_t flags) {
    nx_arch_irq_state_t token = nx_arch_irq_save();
    nx_uart_port_t* port = &g_nx_native_uart;
    if (!port->opened || port->stopping) {
        nx_arch_irq_restore(token);
        return NX_ERROR_STATE;
    }
    if ((flags & NX_UART_EVENT_NO_BYTE) != 0 &&
        port->config.profile == NX_UART_RX_BYTES) {
        port->loss = true;
        nx_arch_irq_restore(token);
        return NX_SUCCESS;
    }
    if (port->rx_count == port->config.rx_capacity ||
        (port->config.profile == NX_UART_RX_EVENTS && port->loss)) {
        if (!port->loss) {
            port->loss_timestamp = nx_time_now_us();
        }
        port->loss = true;
        nx_arch_irq_restore(token);
        return NX_ERROR_OVERFLOW;
    }
    size_t tail = port->rx_head + port->rx_count;
    if (tail >= port->config.rx_capacity) {
        tail -= port->config.rx_capacity;
    }
    if (port->config.profile == NX_UART_RX_EVENTS) {
        nx_uart_rx_event_t* storage = port->config.rx_storage;
        storage[tail] = (nx_uart_rx_event_t){
            nx_time_now_us(), flags,
            (flags & NX_UART_EVENT_NO_BYTE) != 0 ? 0 : byte};
    } else {
        uint8_t* storage = port->config.rx_storage;
        storage[tail] = byte;
        if (flags != 0) {
            port->loss = true;
        }
    }
    port->rx_count++;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Pop at most one event per short metadata critical section. */
nx_result_t nx_uart_port_read_events(nx_uart_port_t* port,
                                     nx_uart_rx_event_t* events,
                                     size_t capacity, size_t* count) {
    if (port == NULL || events == NULL || capacity == 0 || count == NULL) {
        return NX_ERROR_INVALID;
    }
    *count = 0;
    if (port->config.profile != NX_UART_RX_EVENTS) {
        return NX_ERROR_UNSUPPORTED;
    }
    while (*count < capacity) {
        nx_arch_irq_state_t token = nx_arch_irq_save();
        if (port->rx_count == 0) {
            if (port->loss) {
                events[*count] = (nx_uart_rx_event_t){
                    port->loss_timestamp,
                    NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE, 0};
                port->loss = false;
                nx_arch_irq_restore(token);
                (*count)++;
            } else {
                nx_arch_irq_restore(token);
            }
            break;
        }
        nx_uart_rx_event_t* storage = port->config.rx_storage;
        events[*count] = storage[port->rx_head];
        port->rx_head++;
        if (port->rx_head == port->config.rx_capacity) {
            port->rx_head = 0;
        }
        port->rx_count--;
        nx_arch_irq_restore(token);
        (*count)++;
    }
    return *count == 0 ? NX_ERROR_EMPTY : NX_SUCCESS;
}

/** \brief Preserve explicit aggregate loss while copying the selected byte
 * ring. */
nx_result_t nx_uart_port_read_bytes(nx_uart_port_t* port, uint8_t* bytes,
                                    size_t capacity, size_t* count) {
    if (port == NULL || bytes == NULL || capacity == 0 || count == NULL) {
        return NX_ERROR_INVALID;
    }
    *count = 0;
    if (port->config.profile != NX_UART_RX_BYTES) {
        return NX_ERROR_UNSUPPORTED;
    }
    bool loss = false;
    while (*count < capacity) {
        nx_arch_irq_state_t token = nx_arch_irq_save();
        loss = loss || port->loss;
        port->loss = false;
        if (port->rx_count == 0) {
            nx_arch_irq_restore(token);
            break;
        }
        uint8_t* storage = port->config.rx_storage;
        bytes[*count] = storage[port->rx_head++];
        if (port->rx_head == port->config.rx_capacity) {
            port->rx_head = 0;
        }
        port->rx_count--;
        nx_arch_irq_restore(token);
        (*count)++;
    }
    if (loss) {
        return NX_ERROR_OVERFLOW;
    }
    return *count == 0 ? NX_ERROR_EMPTY : NX_SUCCESS;
}

/** \brief Reject new work while the execution owner continues cancel/drain. */
nx_result_t nx_uart_port_stop(nx_uart_port_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t stop_token = nx_arch_irq_save();
    port->stopping = true;
    bool active = port->active != NULL;
    if (active && !port->terminal) {
        port->terminal = true;
        port->terminal_result = NX_ERROR_CANCELLED;
    }
    nx_arch_irq_restore(stop_token);
    if (active) {
        nx_uart_port_service(port);
        if (port->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    port->opened = false;
    port->rx_count = 0;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Inject only controlled fixture faults; do not revoke live storage. */
void nx_native_uart_fault(bool start_failure, bool hold_drain) {
    nx_arch_irq_state_t token = nx_arch_irq_save();
    g_nx_native_uart.start_failure = start_failure;
    g_nx_native_uart.hold_drain = hold_drain;
    nx_arch_irq_restore(token);
}

/** \brief Read capture length under the same model IRQ exclusion discipline. */
size_t nx_native_uart_transmitted(void) {
    nx_arch_irq_state_t token = nx_arch_irq_save();
    size_t logged = g_nx_native_uart.logged;
    nx_arch_irq_restore(token);
    return logged;
}
