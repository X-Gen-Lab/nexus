/** Synchronous facade uses the same IT runtime, ownership and total deadline.
 * The facade polls; it never blocks the RX IRQ with HAL_UART_Receive(). */
#include "stm32_uart_runtime.h"
#include <limits.h>
static nx_status_t send(nx_tx_sync_t* self, const uint8_t* data, size_t length,
                         uint32_t timeout_ms) {
    if (!self) return NX_ERR_NULL_PTR;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, tx_sync);
    nx_uart_ticket_t ticket;
    nx_status_t status = impl->operations.submit(&impl->operations, data, length,
                                                 timeout_ms, &ticket);
    if (status != NX_OK) return status;
    nx_uart_result_t result;
    do {
        status = impl->operations.poll(&impl->operations, ticket, &result);
        if (status != NX_OK) return status;
        /* An abort failure cannot cause this wrapper to return the borrowed
         * buffer. Remain until hardware or a successful stop settles it. */
        if (!result.settled) __NOP();
    } while (!result.settled);
    return result.status;
}
static nx_status_t receive_impl(nx_rx_sync_t* self, uint8_t* data, size_t* length,
                                 uint32_t timeout_ms, bool all) {
    if (!self || !data || !length || !*length || timeout_ms > INT32_MAX)
        return NX_ERR_INVALID_PARAM;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, rx_sync);
    if (!impl->state || !impl->state->initialized) return NX_ERR_NOT_INIT;
    size_t requested = *length; *length = 0;
    uint32_t started = HAL_GetTick();
    do {
        nx_uart_rx_event_t event;
        nx_status_t status = impl->operations.receive_event(&impl->operations, &event);
        if (status == NX_OK) {
            if (event.status != NX_OK) return event.status;
            if (event.has_data) data[(*length)++] = event.data;
            if (*length == requested || (!all && *length)) return NX_OK;
        } else if (status != NX_ERR_NO_DATA) return status;
        if ((uint32_t)(HAL_GetTick() - started) >= timeout_ms) return NX_ERR_TIMEOUT;
        __NOP();
    } while (true);
}
static nx_status_t receive(nx_rx_sync_t* self, uint8_t* data, size_t* length, uint32_t timeout_ms) {
    return receive_impl(self, data, length, timeout_ms, false);
}
static nx_status_t receive_all(nx_rx_sync_t* self, uint8_t* data, size_t* length, uint32_t timeout_ms) {
    return receive_impl(self, data, length, timeout_ms, true);
}
void uart_init_tx_sync(nx_tx_sync_t* self) { self->send = send; }
void uart_init_rx_sync(nx_rx_sync_t* self) { self->receive = receive; self->receive_all = receive_all; }
