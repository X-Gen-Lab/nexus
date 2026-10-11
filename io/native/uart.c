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
#include "provider.h"

static nx_native_uart_state_t s_uart;
const nx_uart_port_t g_nx_native_uart = {&nx_native_uart_ops, &s_uart};
const nx_uart_port_t* const nx_native_uart = &g_nx_native_uart;

/** \brief Validate fixed-port preparation before establishing any borrow. */
static nx_result_t validate_start(nx_native_uart_state_t* port,
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
static void attach_request(nx_native_uart_state_t* port,
                           nx_uart_tx_request_t* request) {
    port->active = request;
    port->tx_index = 0;
    port->terminal = port->start_failure;
    port->terminal_result = port->start_failure ? NX_ERROR_IO : NX_SUCCESS;
}

/** \brief Configure only after the previous provider borrow is gone. */
nx_result_t
nx_native_uart_configure_instance(nx_native_uart_state_t* port,
                                  const nx_native_uart_config_t* config) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
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
    if (port->active != NULL || port->rx_stream != NULL) {
        nx_arch_irq_restore(token);
        return NX_ERROR_BUSY;
    }
    *port = (nx_native_uart_state_t){0};
    port->config = *config;
    port->irq_policy = (nx_irq_policy_t){NX_IRQ_KERNEL_BASEPRI, 240U, 4U, 5U};
    port->irq_source = (nx_irq_source_t){0, 5U, 0U};
    port->opened = true;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Configure only explicit, quiescent model facts. */
nx_result_t nx_native_uart_model_irq_configure(const nx_uart_port_t* binding,
                                               const nx_irq_policy_t* policy,
                                               const nx_irq_source_t* source) {
    if (binding == NULL || binding->ops != &nx_native_uart_ops ||
        binding->context == NULL ||
        nx_irq_wake_validate(NULL, policy, source) != NX_SUCCESS) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_native_uart_state_t* port = binding->context;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (!port->opened || port->stopping) {
        result = NX_ERROR_STATE;
    } else if (port->active != NULL || port->rx_stream != NULL ||
               port->wake != NULL) {
        result = NX_ERROR_BUSY;
    } else {
        port->irq_policy = *policy;
        port->irq_source = *source;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Operate on the explicit default fixture only. */
nx_result_t nx_native_uart_configure(const nx_native_uart_config_t* config) {
    return nx_native_uart_configure_instance(&s_uart, config);
}

/** \brief Direct admission occurs once, after every rejectable validation. */
static nx_result_t native_uart_submit(void* context,
                                      nx_uart_tx_request_t* request) {
    nx_native_uart_state_t* port = context;
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
static nx_result_t native_uart_start_admitted(void* context,
                                              nx_uart_tx_request_t* request) {
    nx_native_uart_state_t* port = context;
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
static nx_result_t native_uart_cancel(void* context,
                                      nx_uart_tx_request_t* request) {
    nx_native_uart_state_t* port = context;
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
void nx_native_uart_irq_step_instance(nx_native_uart_state_t* port) {
    if (port == NULL) {
        return;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();

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
    const nx_irq_wake_t* wake = port->terminal ? port->wake : NULL;
    nx_arch_irq_restore(token);
    (void)nx_irq_wake_signal(wake);
}

/** \brief Operate on the explicit default fixture only. */
void nx_native_uart_irq_step(void) {
    nx_native_uart_irq_step_instance(&s_uart);
}

/** \brief Detach before terminal publication and retain failed drain ownership.
 */
static void native_uart_service(void* context) {
    nx_native_uart_state_t* port = context;
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
        nx_native_uart_irq_step_instance(port);
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
nx_result_t nx_native_uart_receive_instance(nx_native_uart_state_t* port,
                                            uint8_t byte, uint32_t flags) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->rx_stream != NULL) {
        return nx_native_uart_block_receive(port, byte, flags);
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();

    if (!port->opened || port->stopping) {
        nx_arch_irq_restore(token);
        return NX_ERROR_STATE;
    }
    if ((flags & NX_UART_EVENT_NO_BYTE) != 0 &&
        port->config.profile == NX_UART_RX_BYTES) {
        port->loss = true;
        const nx_irq_wake_t* wake = port->wake;
        nx_arch_irq_restore(token);
        (void)nx_irq_wake_signal(wake);
        return NX_SUCCESS;
    }
    if (port->rx_count == port->config.rx_capacity ||
        (port->config.profile == NX_UART_RX_EVENTS && port->loss)) {
        if (!port->loss) {
            port->loss_timestamp = nx_time_now_us();
        }
        port->loss = true;
        const nx_irq_wake_t* wake = port->wake;
        nx_arch_irq_restore(token);
        (void)nx_irq_wake_signal(wake);
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
    const nx_irq_wake_t* wake = port->wake;
    nx_arch_irq_restore(token);
    (void)nx_irq_wake_signal(wake);
    return NX_SUCCESS;
}

/** \brief Operate on the explicit default fixture only. */
nx_result_t nx_native_uart_receive(uint8_t byte, uint32_t flags) {
    return nx_native_uart_receive_instance(&s_uart, byte, flags);
}

/** \brief Pop at most one event per short metadata critical section. */
static nx_result_t native_uart_read_events(void* context,
                                           nx_uart_rx_event_t* events,
                                           size_t capacity, size_t* count) {
    nx_native_uart_state_t* port = context;
    if (port == NULL || events == NULL || capacity == 0 || count == NULL) {
        return NX_ERROR_INVALID;
    }
    *count = 0;
    if (port->rx_stream != NULL || port->config.profile != NX_UART_RX_EVENTS) {
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
static nx_result_t native_uart_read_bytes(void* context, uint8_t* bytes,
                                          size_t capacity, size_t* count) {
    nx_native_uart_state_t* port = context;
    if (port == NULL || bytes == NULL || capacity == 0 || count == NULL) {
        return NX_ERROR_INVALID;
    }
    *count = 0;
    if (port->rx_stream != NULL || port->config.profile != NX_UART_RX_BYTES) {
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
static nx_result_t native_uart_stop(void* context) {
    nx_native_uart_state_t* port = context;
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
        native_uart_service(port);
        if (port->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    nx_result_t rx_result = nx_native_uart_rx_stop(port);
    if (rx_result != NX_SUCCESS) {
        return rx_result;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    port->wake = NULL;
    port->opened = false;
    port->rx_count = 0;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Inject only controlled fixture faults; do not revoke live storage. */
void nx_native_uart_fault_instance(nx_native_uart_state_t* port,
                                   bool start_failure, bool hold_drain) {
    if (port == NULL) {
        return;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    port->start_failure = start_failure;
    port->hold_drain = hold_drain;
    nx_arch_irq_restore(token);
}

/** \brief Operate on the explicit default fixture only. */
void nx_native_uart_fault(bool start_failure, bool hold_drain) {
    nx_native_uart_fault_instance(&s_uart, start_failure, hold_drain);
}

/** \brief Read capture length under the same model IRQ exclusion discipline. */
size_t nx_native_uart_transmitted_instance(const nx_native_uart_state_t* port) {
    if (port == NULL) {
        return 0;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    size_t logged = port->logged;
    nx_arch_irq_restore(token);
    return logged;
}

/** \brief Operate on the explicit default fixture only. */
size_t nx_native_uart_transmitted(void) {
    return nx_native_uart_transmitted_instance(&s_uart);
}

/** \brief Bind an explicit sink using the maintained model IRQ priority. */
static nx_result_t native_uart_attach_wake(void* context,
                                           const nx_irq_wake_t* wake,
                                           uint8_t syscall_ceiling) {
    nx_native_uart_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!port->opened) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_native_irq_wake_validate(
        wake, &port->irq_policy, &port->irq_source, syscall_ceiling);
    if (result == NX_SUCCESS) {
        port->wake = wake;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief One readonly operation table is shared by every Native instance. */
const nx_uart_ops_t nx_native_uart_ops = {
    .submit = native_uart_submit,
    .start_admitted = native_uart_start_admitted,
    .cancel = native_uart_cancel,
    .service = native_uart_service,
    .read_events = native_uart_read_events,
    .read_bytes = native_uart_read_bytes,
    .stop = native_uart_stop,
    .attach_wake = native_uart_attach_wake,
    .rx_start = nx_native_uart_rx_start,
    .rx_stop = nx_native_uart_rx_stop,
};

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t
nx_native_uart_model_configure(const nx_uart_port_t* binding,
                               const nx_native_uart_config_t* config) {
    if (binding == NULL || binding->ops != &nx_native_uart_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_native_uart_configure_instance(binding->context, config);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
void nx_native_uart_model_irq_step(const nx_uart_port_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_uart_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_uart_irq_step_instance(binding->context);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t nx_native_uart_model_receive(const nx_uart_port_t* binding,
                                         uint8_t byte, uint32_t flags) {
    if (binding == NULL || binding->ops != &nx_native_uart_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_native_uart_receive_instance(binding->context, byte, flags);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
void nx_native_uart_model_fault(const nx_uart_port_t* binding,
                                bool start_failure, bool hold_drain) {
    if (binding == NULL || binding->ops != &nx_native_uart_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_uart_fault_instance(binding->context, start_failure, hold_drain);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
size_t nx_native_uart_model_transmitted(const nx_uart_port_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_uart_ops ||
        binding->context == NULL) {
        return 0;
    }
    return nx_native_uart_transmitted_instance(binding->context);
}
