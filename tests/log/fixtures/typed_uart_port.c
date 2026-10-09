#include "hal/provider/nx_device_provider.h"
#include "typed_uart_port.h"
#include "arch/nx_arch.h"
#include <assert.h>
#include <string.h>
typed_uart_port_t uart_port;
static nx_status_t init(nx_lifecycle_t* self) {
    (void)self; assert(!nx_arch_irq_is_masked());
    uart_port.hardware = NX_DEV_STATE_RUNNING; return NX_OK;
}
static nx_status_t deinit(nx_lifecycle_t* self) {
    (void)self; assert(!nx_arch_irq_is_masked()); ++uart_port.closes;
    if (uart_port.close_status == NX_OK) {
        uart_port.hardware = NX_DEV_STATE_UNINITIALIZED; uart_port.borrowed = NULL;
    }
    return uart_port.close_status;
}
static nx_device_state_t state(nx_lifecycle_t* self) { (void)self; return uart_port.hardware; }
static nx_lifecycle_t* lifecycle(nx_uart_t* self) { (void)self; return &uart_port.life; }
static nx_status_t construct(const nx_device_t* dev, void** out) {
    (void)dev; assert(!nx_arch_irq_is_masked()); *out = &uart_port.uart; return NX_OK;
}
static nx_status_t submit(nx_uart_operations_t* self, const uint8_t* data, size_t len,
                           uint32_t budget, nx_uart_ticket_t* ticket) {
    (void)self; assert(budget && !nx_arch_irq_is_masked());
    if (uart_port.borrowed) return NX_ERR_BUSY;
    uart_port.borrowed = data; uart_port.borrowed_length = len; ++uart_port.submits;
    ticket->sequence = ++uart_port.sequence;
    if (uart_port.zero_ticket) ticket->sequence = 0;
    return NX_OK;
}
static nx_status_t poll(nx_uart_operations_t* self, nx_uart_ticket_t ticket, nx_uart_result_t* out) {
    (void)self; assert(!nx_arch_irq_is_masked());
    if (ticket.sequence != uart_port.sequence) return NX_ERR_INVALID_STATE;
    if (uart_port.poll_status != NX_OK) return uart_port.poll_status;
    bool complete = uart_port.complete || uart_port.auto_complete;
    bool idle = uart_port.wire_idle || uart_port.auto_complete;
    *out = (nx_uart_result_t){.status = complete ? uart_port.terminal_status : NX_ERR_BUSY,
        .settled = complete, .wire_idle = idle, .transferred = complete ? uart_port.borrowed_length : 0};
    if (complete) uart_port.borrowed = NULL;
    return NX_OK;
}
static nx_status_t cancel(nx_uart_operations_t* self, nx_uart_ticket_t ticket) {
    (void)self; assert(!nx_arch_irq_is_masked()); ++uart_port.cancellations;
    if (ticket.sequence != uart_port.sequence) return NX_ERR_INVALID_STATE;
    if (uart_port.cancel_status == NX_OK) uart_port.borrowed = NULL;
    return uart_port.cancel_status;
}
static nx_status_t receive(nx_uart_operations_t* self, nx_uart_rx_event_t* out) {
    (void)self;
    if (!uart_port.rx_error) return NX_ERR_NO_DATA;
    *out = (nx_uart_rx_event_t){.status = NX_ERR_PARITY, .raw_error = 1};
    return NX_OK;
}
static nx_uart_operations_t* operations(nx_uart_t* self) { (void)self; return &uart_port.ops; }
void typed_uart_port_reset(void) {
    assert(nx_device_registry_reset() == NX_OK);
    memset(&uart_port, 0, sizeof(uart_port));
    uart_port.hardware = NX_DEV_STATE_UNINITIALIZED;
    uart_port.life = (nx_lifecycle_t){.init = init, .deinit = deinit, .get_state = state};
    uart_port.uart.get_lifecycle = lifecycle; uart_port.uart.get_operations = operations;
    uart_port.ops = (nx_uart_operations_t){.submit = submit, .poll = poll, .cancel = cancel, .receive_event = receive};
    uart_port.descriptor = (nx_device_t){.name = "UART_TEST", .state = &uart_port.state,
        .device_class = NX_DEVICE_CLASS_UART, .construct = construct};
    assert(nx_device_register(&uart_port.descriptor) == NX_OK);
}
