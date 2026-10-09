/** Typed Native UART: copy bytes into a bounded observation buffer and settle
 * immediately. No serial device, wire speed, DMA or hidden worker is implied.
 * RX records are supplied by an explicit external host-model producer; the
 * producer timestamps insertion, not the consumer drain. */
#include "arch/nx_arch.h"
#include "nx_uart_helpers.h"
#include "osal/osal.h"
#include <limits.h>
#include <string.h>

static nx_status_t context_ok(void) {
    return nx_arch_in_isr() || nx_arch_irq_is_masked() ? NX_ERR_INVALID_STATE
                                                       : NX_OK;
}
static nx_status_t ready(nx_uart_impl_t* impl) {
    return !impl->state || !impl->state->initialized ? NX_ERR_NOT_INIT
           : impl->state->suspended                  ? NX_ERR_INVALID_STATE
                                                     : NX_OK;
}
static nx_status_t submit(nx_uart_operations_t* self, const uint8_t* data,
                          size_t length, uint32_t timeout_ms,
                          nx_uart_ticket_t* ticket) {
    if (!ticket)
        return NX_ERR_NULL_PTR;
    ticket->sequence = 0;
    if (!self || !data || !length || timeout_ms > INT32_MAX)
        return NX_ERR_INVALID_PARAM;
    nx_status_t r = context_ok();
    if (r != NX_OK)
        return r;
    if (!timeout_ms)
        return NX_ERR_TIMEOUT;
    uint32_t started = 0;
    if (osal_get_time_ms(&started) != OSAL_OK)
        return NX_ERR_NOT_INIT;
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, operations);
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    r = ready(impl);
    if (r != NX_OK) {
        nx_arch_irq_restore(saved);
        return r;
    }
    nx_uart_buffer_t* buffer = &impl->state->tx_buf;
    if (!buffer->data || !buffer->size || buffer->count > buffer->size)
        r = NX_ERR_INVALID_STATE;
    if (r == NX_OK && impl->state->tx_busy)
        r = NX_ERR_BUSY;
    if (r == NX_OK && impl->sequence == UINT64_MAX)
        r = NX_ERR_NO_RESOURCE;
    if (r == NX_OK && length > buffer->size - buffer->count)
        r = NX_ERR_FULL;
    size_t head = buffer->head;
    if (r == NX_OK)
        impl->state->tx_busy = true;
    nx_arch_irq_restore(saved);
    if (r != NX_OK)
        return r;
    /* Payload copy runs outside metadata masks. Legacy writers check tx_busy;
     * typed dispatch pins lifetime. The observation consumer is model-owned. */
    size_t first = buffer->size - head;
    if (first > length)
        first = length;
    memcpy(buffer->data + head, data, first);
    if (length > first)
        memcpy(buffer->data, data + first, length - first);
    uint32_t ended = started;
    nx_status_t terminal =
        osal_get_time_ms(&ended) == OSAL_OK
            ? (uint32_t)(ended - started) >= timeout_ms ? NX_ERR_TIMEOUT : NX_OK
            : NX_ERR_IO;
    saved = nx_arch_irq_save();
    buffer->head = (head + length) % buffer->size;
    buffer->count += length;
    uint32_t* count = &impl->state->stats.tx_count;
    *count =
        length > UINT32_MAX - *count ? UINT32_MAX : *count + (uint32_t)length;
    impl->result = (nx_uart_result_t){terminal, length, true, true};
    ticket->sequence = ++impl->sequence;
    impl->state->tx_busy = false;
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t poll(nx_uart_operations_t* self, nx_uart_ticket_t ticket,
                        nx_uart_result_t* result) {
    if (!result)
        return NX_ERR_NULL_PTR;
    *result = (nx_uart_result_t){.status = NX_ERR_INVALID_STATE};
    if (!self || !ticket.sequence)
        return NX_ERR_INVALID_PARAM;
    nx_status_t r = context_ok();
    if (r != NX_OK)
        return r;
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, operations);
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    r = ready(impl);
    if (r == NX_OK && ticket.sequence != impl->sequence)
        r = NX_ERR_INVALID_STATE;
    if (r == NX_OK)
        *result = impl->result;
    nx_arch_irq_restore(saved);
    return r;
}
static nx_status_t cancel(nx_uart_operations_t* self, nx_uart_ticket_t ticket) {
    nx_uart_result_t result;
    nx_status_t r = poll(self, ticket, &result);
    /* The copy already settled; cancellation cannot undo accepted bytes. */
    return r == NX_OK && !result.settled ? NX_ERR_BUSY : r;
}
static nx_status_t receive(nx_uart_operations_t* self,
                           nx_uart_rx_event_t* event) {
    if (!event)
        return NX_ERR_NULL_PTR;
    *event = (nx_uart_rx_event_t){.status = NX_ERR_NO_DATA};
    if (!self)
        return NX_ERR_INVALID_PARAM;
    nx_status_t r = context_ok();
    if (r != NX_OK)
        return r;
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, operations);
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    r = ready(impl);
    if (r == NX_OK) {
        if (impl->rx_event_count) {
            *event = impl->rx_events[impl->rx_event_head];
            impl->rx_event_head =
                (impl->rx_event_head + 1) % impl->rx_event_capacity;
            --impl->rx_event_count;
        } else if (impl->rx_event_dropped) {
            *event =
                (nx_uart_rx_event_t){.status = NX_ERR_FULL,
                                     .timestamp_us = impl->rx_drop_timestamp,
                                     .resolution_us = 1000,
                                     .raw_error = impl->rx_event_dropped};
            impl->rx_event_dropped = 0;
        } else
            r = NX_ERR_NO_DATA;
    }
    nx_arch_irq_restore(saved);
    return r;
}
static nx_uart_operations_t* operations(nx_uart_t* self) {
    return self ? &uart_get_impl(self)->operations : NULL;
}
void native_uart_init_operations(nx_uart_impl_t* impl) {
    impl->operations = (nx_uart_operations_t){.submit = submit,
                                              .poll = poll,
                                              .cancel = cancel,
                                              .receive_event = receive};
    impl->base.get_operations = operations;
}
