#ifndef TYPED_UART_PORT_H
#define TYPED_UART_PORT_H
#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
typedef struct {
    nx_uart_t uart;
    nx_lifecycle_t life;
    nx_uart_operations_t ops;
    nx_device_config_state_t state;
    nx_device_t descriptor;
    nx_device_state_t hardware;
    nx_status_t close_status, cancel_status, poll_status, terminal_status;
    bool complete, wire_idle, auto_complete, zero_ticket, rx_error;
    const uint8_t* borrowed;
    size_t borrowed_length;
    uint64_t sequence;
    unsigned submits, closes, cancellations;
} typed_uart_port_t;
extern typed_uart_port_t uart_port;
void typed_uart_port_reset(void);
#endif
