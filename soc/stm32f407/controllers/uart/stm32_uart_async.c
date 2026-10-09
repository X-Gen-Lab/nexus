/** Bounded interrupt UART runtime. DMA is rejected until a real board allocation
 * exists. One zero-copy operation; task code must poll to enforce its budget. */
#include "stm32_uart_runtime.h"
#include "stm32_uart_helpers.h"
#include <limits.h>
#include <string.h>

static nx_arch_irq_state_t lock(void) { return nx_arch_irq_save(); }
static void unlock(nx_arch_irq_state_t key) { nx_arch_dmb(); nx_arch_irq_restore(key); }
static bool wire_idle(stm32_uart_impl_t* impl) {
    return __HAL_UART_GET_FLAG(&impl->huart, UART_FLAG_TC) != RESET ||
           !(impl->huart.Instance->CR1 & USART_CR1_UE);
}

/* Internal helper may run inside the short legacy copy critical section.
 * Every public entry validates its original caller context before masking. */
static nx_status_t submit_request(nx_uart_operations_t* self, const uint8_t* data,
                           size_t length, uint32_t timeout_ms,
                           nx_uart_ticket_t* ticket) {
    if (!self || !ticket || !data || !length || length > UINT16_MAX ||
        timeout_ms > INT32_MAX) return NX_ERR_INVALID_PARAM;
    ticket->sequence = 0;
    if (!timeout_ms) return NX_ERR_TIMEOUT;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, operations);
    nx_arch_irq_state_t key = lock();
    if (!impl->state || !impl->state->initialized) { unlock(key); return NX_ERR_NOT_INIT; }
    if (impl->state->suspended || impl->faulted) { unlock(key); return NX_ERR_INVALID_STATE; }
    if (impl->callback_active || impl->closing || impl->state->tx_busy ||
        (impl->ticket.sequence && !wire_idle(impl))) {
        unlock(key); return NX_ERR_BUSY;
    }
    if (impl->ticket.sequence == UINT64_MAX) { unlock(key); return NX_ERR_NO_RESOURCE; }
    impl->ticket.sequence++;
    impl->ticket_active = false;
    impl->submitted_ms = HAL_GetTick();
    impl->timeout_ms = timeout_ms;
    impl->tx_length = length;
    impl->result = (nx_uart_result_t){.status = NX_ERR_BUSY};
    impl->state->tx_busy = true;
    impl->notification_pending = false;
    HAL_StatusTypeDef status = HAL_UART_Transmit_IT(&impl->huart, (uint8_t*)data,
                                                   (uint16_t)length);
    if (status != HAL_OK) {
        impl->state->tx_busy = false;
        impl->result.status = stm32_uart_hal_to_nx_status(status);
        impl->result.settled = true;
        impl->result.wire_idle = wire_idle(impl);
        unlock(key); return impl->result.status;
    }
    *ticket = impl->ticket;
    impl->ticket_active = true;
    unlock(key);
    return NX_OK;
}

static nx_status_t submit(nx_uart_operations_t* self, const uint8_t* data,
                           size_t length, uint32_t timeout_ms, nx_uart_ticket_t* ticket) {
    if (ticket) ticket->sequence = 0;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    return submit_request(self, data, length, timeout_ms, ticket);
}

static nx_status_t stop(stm32_uart_impl_t* impl, nx_uart_ticket_t ticket,
                         nx_status_t terminal_status) {
    nx_arch_irq_state_t key = lock();
    if (!impl->ticket_active || !ticket.sequence || ticket.sequence != impl->ticket.sequence) {
        unlock(key); return NX_ERR_INVALID_STATE;
    }
    if (impl->callback_active || impl->closing) { unlock(key); return NX_ERR_BUSY; }
    if (!impl->state->tx_busy) {
        /* Cancel may race the completion IRQ before task delivery. Settlement
         * forbids dispatching this old callback after returning success. */
        impl->notification_pending = false;
        unlock(key); return NX_OK;
    }
    /* Supported profile uses IT only. HAL abort disables all TX interrupt
     * sources synchronously; no DMA request or borrowed DMA buffer exists. */
    size_t transferred = impl->tx_length - impl->huart.TxXferCount;
    HAL_StatusTypeDef status = HAL_UART_AbortTransmit(&impl->huart);
    if (status != HAL_OK) {
        /* Deterministic IT-only quarantine: suppress every source that can
         * dereference the buffer, stop UE, then publish settlement. This may
         * truncate a frame. The failure remains visible and new I/O is denied. */
        CLEAR_BIT(impl->huart.Instance->CR1, USART_CR1_TXEIE | USART_CR1_TCIE);
        __HAL_UART_DISABLE(&impl->huart);
        nx_arch_dsb();
        impl->huart.pTxBuffPtr = NULL;
        impl->state->tx_busy = false;
        impl->faulted = true;
        impl->rx_fault_pending = true;
        impl->rx_fault_timestamp = stm32_uart_board_timestamp_us();
        impl->result = (nx_uart_result_t){.status = NX_ERR_HARDWARE,
            .transferred = transferred, .settled = true, .wire_idle = true};
        impl->notification_pending = true;
        unlock(key); return stm32_uart_hal_to_nx_status(status);
    }
    nx_arch_dsb();
    impl->huart.pTxBuffPtr = NULL;
    impl->state->tx_busy = false;
    impl->result = (nx_uart_result_t){.status = terminal_status,
        .transferred = transferred, .settled = true, .wire_idle = wire_idle(impl)};
    impl->notification_pending = true;
    unlock(key);
    return NX_OK;
}

static nx_status_t cancel(nx_uart_operations_t* self, nx_uart_ticket_t ticket) {
    if (!self) return NX_ERR_NULL_PTR;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, operations);
    if (!impl->state || !impl->state->initialized) return NX_ERR_NOT_INIT;
    return stop(impl, ticket, NX_ERR_CANCELLED);
}
static nx_status_t poll(nx_uart_operations_t* self, nx_uart_ticket_t ticket,
                         nx_uart_result_t* result) {
    if (!self || !result) return NX_ERR_NULL_PTR;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, operations);
    nx_arch_irq_state_t key = lock();
    if (!impl->state || !impl->state->initialized) { unlock(key); return NX_ERR_NOT_INIT; }
    if (!impl->ticket_active || !ticket.sequence || ticket.sequence != impl->ticket.sequence) {
        unlock(key); return NX_ERR_INVALID_STATE;
    }
    if (impl->callback_active || impl->closing) { unlock(key); return NX_ERR_BUSY; }
    bool expired = impl->state->tx_busy &&
        (uint32_t)(HAL_GetTick() - impl->submitted_ms) >= impl->timeout_ms;
    unlock(key);
    if (expired) (void)stop(impl, ticket, NX_ERR_TIMEOUT);
    key = lock();
    if (!impl->state || !impl->state->initialized) { unlock(key); return NX_ERR_NOT_INIT; }
    if (!impl->ticket_active || ticket.sequence != impl->ticket.sequence) {
        unlock(key); return NX_ERR_INVALID_STATE;
    }
    if (impl->callback_active || impl->closing) { unlock(key); return NX_ERR_BUSY; }
    impl->result.wire_idle = !impl->state->tx_busy && wire_idle(impl);
    *result = impl->result;
    bool notify = impl->notification_pending;
    impl->notification_pending = false;
    void (*callback)(void*) = impl->callbacks.tx_complete_cb;
    void* context = impl->callbacks.user_data;
    notify = notify && result->status == NX_OK && callback;
    impl->callback_active = notify;
    unlock(key);
    /* Deferred notification in task context, exactly once. */
    if (notify) {
        callback(context);
        key = lock(); impl->callback_active = false; unlock(key);
    }
    return NX_OK;
}
void stm32_uart_tx_completed(stm32_uart_impl_t* impl) {
    if (!impl || !impl->state || !impl->state->initialized ||
        !impl->state->tx_busy || !wire_idle(impl) || impl->huart.TxXferCount) return;
    /* ST F4 normal-mode IT TX callback is reached at UART TC, not TXE. */
    impl->huart.pTxBuffPtr = NULL;
    impl->state->tx_busy = false;
    impl->result = (nx_uart_result_t){.status = NX_OK,
        .transferred = impl->tx_length, .settled = true, .wire_idle = true};
    impl->notification_pending = true;
}
void stm32_uart_rx_event(stm32_uart_impl_t* impl, bool has_data, uint8_t data,
                          nx_status_t status, uint32_t raw_error) {
    if (!impl || !impl->rx_capacity || impl->faulted || impl->closing) return;
    nx_arch_irq_state_t key = lock();
    if (impl->rx_count == impl->rx_capacity || impl->rx_lost) {
        if (!impl->rx_lost) impl->rx_lost_timestamp = stm32_uart_board_timestamp_us();
        if (impl->rx_lost != UINT32_MAX) impl->rx_lost++;
        unlock(key); return;
    }
    nx_uart_rx_event_t event = {.timestamp_us = stm32_uart_board_timestamp_us(),
        .resolution_us = stm32_uart_board_timestamp_resolution_us(),
        .raw_error = raw_error, .data = data, .has_data = has_data, .status = status};
    impl->rx_events[impl->rx_head] = event;
    impl->rx_head = (impl->rx_head + 1) % impl->rx_capacity;
    impl->rx_count++;
    unlock(key);
}
static nx_status_t receive_event(nx_uart_operations_t* self, nx_uart_rx_event_t* event) {
    if (!self || !event) return NX_ERR_NULL_PTR;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, operations);
    nx_arch_irq_state_t key = lock();
    if (!impl->state || !impl->state->initialized) { unlock(key); return NX_ERR_NOT_INIT; }
    if (impl->callback_active || impl->closing) { unlock(key); return NX_ERR_BUSY; }
    if (!impl->rx_count && impl->rx_lost) {
        *event = (nx_uart_rx_event_t){.timestamp_us = impl->rx_lost_timestamp,
            .resolution_us = stm32_uart_board_timestamp_resolution_us(),
            .status = NX_ERR_OVERRUN, .raw_error = impl->rx_lost};
        impl->rx_lost = 0;
        unlock(key); return NX_OK;
    }
    if (!impl->rx_count && impl->rx_fault_pending) {
        /* Preserve every pre-fault event and any earlier overflow marker,
         * then publish the receiver discontinuity caused by disabling UE. */
        *event = (nx_uart_rx_event_t){.timestamp_us = impl->rx_fault_timestamp,
            .resolution_us = stm32_uart_board_timestamp_resolution_us(),
            .status = NX_ERR_HARDWARE};
        impl->rx_fault_pending = false;
    } else if (!impl->rx_count) {
        nx_status_t status = impl->faulted ? NX_ERR_HARDWARE : NX_ERR_NO_DATA;
        unlock(key); return status;
    } else {
        *event = impl->rx_events[impl->rx_tail];
        impl->rx_tail = (impl->rx_tail + 1) % impl->rx_capacity;
        impl->rx_count--;
    }
    void (*callback)(void*) = impl->callbacks.rx_complete_cb;
    void (*error_callback)(void*, uint32_t) = impl->callbacks.error_cb;
    void* context = impl->callbacks.user_data;
    bool notify_error = event->status != NX_OK && error_callback;
    bool notify_receive = !notify_error && event->has_data && callback;
    impl->callback_active = notify_error || notify_receive;
    unlock(key);
    if (notify_error) error_callback(context, event->raw_error);
    else if (notify_receive) callback(context);
    if (notify_error || notify_receive) {
        key = lock(); impl->callback_active = false; unlock(key);
    }
    return NX_OK;
}
nx_status_t stm32_uart_arm_rx(stm32_uart_impl_t* impl) {
    if (!impl->rx_capacity) return NX_OK;
    return stm32_uart_hal_to_nx_status(HAL_UART_Receive_IT(&impl->huart, &impl->rx_byte, 1));
}
void uart_init_operations(stm32_uart_impl_t* impl) {
    impl->operations = (nx_uart_operations_t){submit, poll, cancel, receive_event};
}

/* Legacy async facade copies into static driver storage. This facade never
 * lends the caller's buffer; typed operations above are explicitly zero-copy. */
static nx_status_t legacy_send(nx_tx_async_t* self, const uint8_t* data, size_t len) {
    if (!self || !data || !len) return NX_ERR_INVALID_PARAM;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, tx_async);
    if (!impl->state || !impl->state->initialized) return NX_ERR_NOT_INIT;
    if (len > impl->state->tx_buf.size) return NX_ERR_INVALID_SIZE;
    nx_arch_irq_state_t key = lock();
    if (impl->callback_active || impl->closing || impl->state->tx_busy) { unlock(key); return NX_ERR_BUSY; }
    memcpy(impl->state->tx_buf.data, data, len);
    nx_uart_ticket_t ticket;
    nx_status_t status = submit_request(&impl->operations, impl->state->tx_buf.data, len,
                                INT32_MAX, &ticket);
    unlock(key);
    return status;
}
static nx_status_t legacy_state(nx_tx_async_t* self) {
    if (!self) return NX_ERR_NULL_PTR;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, tx_async);
    if (!impl->ticket_active) return impl->state->initialized ? NX_OK : NX_ERR_NOT_INIT;
    nx_uart_result_t result;
    nx_status_t status = poll(&impl->operations, impl->ticket, &result);
    return status == NX_OK ? result.status : status;
}
static nx_status_t legacy_receive(nx_rx_async_t* self, uint8_t* data, size_t* len) {
    if (!self || !data || !len || !*len) return NX_ERR_INVALID_PARAM;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, rx_async);
    size_t capacity = *len; *len = 0;
    while (*len < capacity) {
        nx_uart_rx_event_t event;
        nx_status_t status = receive_event(&impl->operations, &event);
        if (status != NX_OK) return *len && status == NX_ERR_NO_DATA ? NX_OK : status;
        if (event.status != NX_OK) return event.status;
        if (event.has_data) data[(*len)++] = event.data;
    }
    return NX_OK;
}
void uart_init_tx_async(nx_tx_async_t* self) { self->send = legacy_send; self->get_state = legacy_state; }
void uart_init_rx_async(nx_rx_async_t* self) { self->receive = legacy_receive; }
