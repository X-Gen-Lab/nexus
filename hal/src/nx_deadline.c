#include "hal/runtime/nx_deadline.h"
#include "arch/nx_arch.h"
#include <limits.h>
#include <string.h>
static bool valid(const nx_hal_wait_port_t* p) {
    return p && p->now_ms && p->wait_ms && p->poll_interval_ms && p->poll_interval_ms <= INT32_MAX;
}
nx_status_t nx_hal_deadline_start(const nx_hal_wait_port_t* port, uint32_t budget,
                                 nx_hal_deadline_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    *out = (nx_hal_deadline_t){0};
    if (!valid(port) || budget > INT32_MAX) return NX_ERR_INVALID_PARAM;
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    nx_status_t s = port->now_ms(port->context, &out->started_ms);
    if (s == NX_OK) out->timeout_ms = budget;
    return s;
}
nx_status_t nx_hal_deadline_remaining(const nx_hal_wait_port_t* port, nx_hal_deadline_t d,
                                     uint32_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    *out = 0;
    if (!valid(port) || d.timeout_ms > INT32_MAX) return NX_ERR_INVALID_PARAM;
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    uint32_t now = 0; nx_status_t s = port->now_ms(port->context, &now);
    if (s == NX_OK) {
        uint32_t elapsed = now - d.started_ms;
        *out = elapsed >= d.timeout_ms ? 0 : d.timeout_ms - elapsed;
    }
    return s;
}
static nx_status_t settle(nx_device_ref_t ref, nx_uart_ticket_t ticket,
                          nx_status_t reason, nx_uart_result_t* result) {
    nx_status_t s = nx_device_uart_cancel(ref, ticket);
    if (s == NX_OK) {
        nx_uart_result_t terminal = {0};
        bool observed = nx_device_uart_poll(ref, ticket, &terminal) == NX_OK && terminal.settled;
        if (observed) *result = terminal;
        result->settled = true;
        if (!observed) result->wire_idle = false;
        result->status = reason;
        return reason;
    }
    result->settled = false;
    result->wire_idle = false;
    result->status = s;
    return s;
}
nx_status_t nx_device_uart_transfer(nx_device_ref_t ref, const uint8_t* data, size_t len,
                                    uint32_t budget, const nx_hal_wait_port_t* port,
                                    nx_uart_ticket_t* ticket, nx_uart_result_t* result) {
    if (!ticket || !result) return NX_ERR_NULL_PTR;
    ticket->sequence = 0;
    memset(result, 0, sizeof(*result)); result->status = NX_ERR_INVALID_STATE;
    nx_hal_deadline_t d; nx_status_t s = nx_hal_deadline_start(port, budget, &d);
    if (s != NX_OK) { result->status = s; result->settled = true; return s; }
    uint32_t left = 0; s = nx_hal_deadline_remaining(port, d, &left);
    if (s != NX_OK || !left) {
        if (s == NX_OK) s = NX_ERR_TIMEOUT;
        result->status = s; result->settled = true; return s;
    }
    s = nx_device_uart_submit(ref, data, len, left, ticket);
    if (s != NX_OK) {
        result->status = s;
        /* INVALID_STATE can denote an admitted zero ticket. Conservatively
         * retain storage until caller diagnoses/retries explicit recovery. */
        result->settled = s != NX_ERR_INVALID_STATE;
        return s;
    }
    for (;;) {
        s = nx_device_uart_poll(ref, *ticket, result);
        if (s != NX_OK) return settle(ref, *ticket, s, result);
        uint32_t remaining = 0;
        s = nx_hal_deadline_remaining(port, d, &remaining);
        if (result->settled) {
            if (s != NX_OK) { result->status = s; return s; }
            if (!remaining && result->status == NX_OK) result->status = NX_ERR_TIMEOUT;
            return result->status;
        }
        if (s != NX_OK) return settle(ref, *ticket, s, result);
        if (!remaining) return settle(ref, *ticket, NX_ERR_TIMEOUT, result);
        uint32_t pause = port->poll_interval_ms < remaining ? port->poll_interval_ms : remaining;
        s = port->wait_ms(port->context, pause);
        if (s != NX_OK) return settle(ref, *ticket, s, result);
    }
}
