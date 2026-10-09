/** Typed UART buffer leases; no OSAL or vendor dependency. */
#include "nx_device_internal.h"
#include "arch/nx_arch.h"
#include <limits.h>
#include <string.h>
static uint32_t metadata_enter(void) { return nx_arch_irq_save().value; }
static void metadata_exit(uint32_t s) { nx_arch_irq_restore((nx_arch_irq_state_t){s}); }
static nx_status_t pin(nx_device_ref_t ref, nx_device_class_t expected, void** api) {
    return nx_device_dispatch_pin(ref, expected, true, api);
}
static void unpin(nx_device_ref_t ref) { nx_device_dispatch_unpin(ref); }
static nx_uart_operations_t* uart_operations(void* api) {
    nx_uart_t* uart = api;
    return uart->get_operations ? uart->get_operations(uart) : NULL;
}
nx_status_t nx_device_uart_submit(nx_device_ref_t ref, const uint8_t* data,
                                  size_t len, uint32_t timeout_ms,
                                  nx_uart_ticket_t* ticket) {
    if (!ticket) return NX_ERR_NULL_PTR;
    ticket->sequence = 0;
    if (!data || !len || timeout_ms > INT32_MAX) return NX_ERR_INVALID_PARAM;
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    uint32_t saved = metadata_enter();
    bool active = ref.descriptor->state->active_ticket != 0;
    metadata_exit(saved);
    nx_uart_operations_t* ops = uart_operations(api);
    if (active) status = NX_ERR_BUSY;
    else if (!ops || !ops->submit || !ops->poll) status = NX_ERR_NOT_SUPPORTED;
    else status = ops->submit(ops, data, len, timeout_ms, ticket);
    if (status == NX_OK) {
        /* A port must never admit a zero ticket. Quarantine rather than close
         * hardware which may have borrowed storage under a broken contract. */
        saved = metadata_enter();
        ref.descriptor->state->active_ticket = ticket->sequence ? ticket->sequence : UINT64_MAX;
        ref.descriptor->state->last_ticket = ticket->sequence;
        if (!ticket->sequence) {
            ref.descriptor->state->uart_unknown_lease = true;
            ref.descriptor->state->phase = NX_DEVICE_RECOVERY_REQUIRED;
            ref.descriptor->state->last_status = NX_ERR_INVALID_STATE;
        }
        metadata_exit(saved);
        if (!ticket->sequence) status = NX_ERR_INVALID_STATE;
    } else ticket->sequence = 0;
    unpin(ref);
    return status;
}
nx_status_t nx_device_uart_poll(nx_device_ref_t ref, nx_uart_ticket_t ticket,
                                nx_uart_result_t* result) {
    if (!result) return NX_ERR_NULL_PTR;
    memset(result, 0, sizeof(*result));
    result->status = NX_ERR_INVALID_STATE;
    if (!ticket.sequence) return NX_ERR_INVALID_PARAM;
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    uint32_t saved = metadata_enter();
    uint64_t active = ref.descriptor->state->active_ticket;
    uint64_t last = ref.descriptor->state->last_ticket;
    metadata_exit(saved);
    nx_uart_operations_t* ops = uart_operations(api);
    if (last != ticket.sequence || (active && active != ticket.sequence)) status = NX_ERR_INVALID_STATE;
    else if (!ops || !ops->poll) status = NX_ERR_NOT_SUPPORTED;
    else status = ops->poll(ops, ticket, result);
    if (status == NX_OK && result->settled && active == ticket.sequence) {
        saved = metadata_enter();
        ref.descriptor->state->active_ticket = 0;
        metadata_exit(saved);
    }
    unpin(ref);
    return status;
}
nx_status_t nx_device_uart_cancel(nx_device_ref_t ref, nx_uart_ticket_t ticket) {
    if (!ticket.sequence) return NX_ERR_INVALID_PARAM;
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    uint32_t saved = metadata_enter();
    uint64_t active = ref.descriptor->state->active_ticket;
    uint64_t last = ref.descriptor->state->last_ticket;
    metadata_exit(saved);
    nx_uart_operations_t* ops = uart_operations(api);
    if (last != ticket.sequence || (active && active != ticket.sequence)) status = NX_ERR_INVALID_STATE;
    else if (!ops || !ops->cancel) status = NX_ERR_NOT_SUPPORTED;
    else status = ops->cancel(ops, ticket);
    if (status == NX_OK && active == ticket.sequence) {
        saved = metadata_enter();
        ref.descriptor->state->active_ticket = 0;
        metadata_exit(saved);
    }
    unpin(ref);
    return status;
}
nx_status_t nx_device_uart_receive_event(nx_device_ref_t ref, nx_uart_rx_event_t* event) {
    if (!event) return NX_ERR_NULL_PTR;
    memset(event, 0, sizeof(*event));
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    nx_uart_operations_t* ops = uart_operations(api);
    status = ops && ops->receive_event ? ops->receive_event(ops, event) : NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}
nx_status_t nx_device_uart_recover(nx_device_ref_t ref) {
    return nx_device_dispatch_recover_unknown_uart(ref);
}
