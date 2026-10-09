/* USART0 IRQ implementation. No DMA or hidden heap allocation. */
#include "hal/provider/nx_device_provider.h"
#include "arch/nx_arch.h"
#include "board.h"
#include "gd32f470_platform.h"
#include "gd32f4xx.h"
#include "hal/base/nx_device.h"
#include "hal/interface/nx_uart.h"
#include "nexus_config.h"
#include <limits.h>

typedef struct {
    nx_uart_t api;
    nx_uart_operations_t operations;
    nx_lifecycle_t lifecycle;
    nx_tx_sync_t tx_sync;
    nx_rx_async_t rx_async;
    nx_device_state_t state;
    const uint8_t* tx;
    size_t tx_length;
    size_t tx_position;
    uint64_t sequence;
    uint32_t start_ms;
    uint32_t timeout_ms;
    nx_uart_result_t result;
    nx_uart_rx_event_t rx[NX_CONFIG_GD32_UART_RX_BUFFER_SIZE];
    size_t head;
    size_t count;
    uint32_t dropped;
    uint64_t overflow_timestamp;
    bool active;
} uart_instance_t;
static uart_instance_t uart;
static nx_device_config_state_t device_state;
/* Backend raw-error bit: controller reset may have interrupted an RX frame. */
#define RX_CONTROLLER_RESET (1u << 31)
static void enqueue_event(nx_uart_rx_event_t event) {
    if (uart.count == NX_CONFIG_GD32_UART_RX_BUFFER_SIZE || uart.dropped) {
        if (!uart.dropped) { uart.overflow_timestamp = event.timestamp_us; }
        if (uart.dropped != UINT32_MAX) { ++uart.dropped; }
    } else {
        size_t tail = (uart.head + uart.count) % NX_CONFIG_GD32_UART_RX_BUFFER_SIZE;
        uart.rx[tail] = event;
        ++uart.count;
    }
}

static void configure_usart(void) {
    usart_baudrate_set(USART0, NX_CONFIG_GD32_UART0_BAUDRATE);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_interrupt_enable(USART0, USART_INT_RBNE);
    usart_interrupt_enable(USART0, USART_INT_ERR);
    usart_interrupt_enable(USART0, USART_INT_PERR);
    usart_enable(USART0);
}

static void settle(nx_status_t status, bool wire_idle) {
    usart_interrupt_disable(USART0, USART_INT_TBE);
    usart_interrupt_disable(USART0, USART_INT_TC);
    uart.tx = NULL;
    uart.result = (nx_uart_result_t){ .status = status,
        .transferred = uart.tx_position, .settled = true, .wire_idle = wire_idle };
    uart.active = false;
    nx_gd32_board_rs485_de(false);
}

static void abort_transfer(nx_status_t status) {
    /* A peripheral reset settles the shifter and all buffer accesses before
     * DE is released. It may truncate a frame and discard an unconsumed byte.
     * Only this task-context path may reset the controller. */
    NVIC_DisableIRQ(USART0_IRQn);
    usart_disable(USART0);
    rcu_periph_reset_enable(RCU_USART0RST);
    rcu_periph_reset_disable(RCU_USART0RST);
    nx_arch_dsb();
    settle(status, true);
    enqueue_event((nx_uart_rx_event_t){ .timestamp_us = nx_gd32f470_timestamp_us(),
        .resolution_us = 1u, .raw_error = RX_CONTROLLER_RESET, .status = NX_ERR_IO });
    NVIC_ClearPendingIRQ(USART0_IRQn);
    configure_usart();
    NVIC_EnableIRQ(USART0_IRQn);
}

static nx_status_t uart_init(nx_lifecycle_t* self) {
    (void)self;
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (uart.active) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    if (uart.state == NX_DEV_STATE_RUNNING) { nx_arch_irq_restore(saved); return NX_OK; }
    rcu_periph_clock_enable(RCU_USART0);
    nx_status_t pins_status = nx_gd32_board_uart_pins(true);
    if (pins_status != NX_OK) { nx_arch_irq_restore(saved); return pins_status; }
    usart_deinit(USART0);
    uart.head = uart.count = 0;
    uart.dropped = 0;
    uart.tx = NULL;
    configure_usart();
    NVIC_ClearPendingIRQ(USART0_IRQn);
    NVIC_SetPriority(USART0_IRQn, 6u);
    uart.state = NX_DEV_STATE_RUNNING;
    NVIC_EnableIRQ(USART0_IRQn);
    nx_arch_irq_restore(saved);
    return NX_OK;
}

static nx_status_t uart_deinit(nx_lifecycle_t* self) {
    (void)self;
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (uart.active) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    NVIC_DisableIRQ(USART0_IRQn);
    usart_disable(USART0);
    usart_deinit(USART0);
    NVIC_ClearPendingIRQ(USART0_IRQn);
    nx_status_t status = nx_gd32_board_uart_pins(false);
    if (status != NX_OK) { nx_arch_irq_restore(saved); return status; }
    uart.head = uart.count = 0;
    uart.dropped = 0;
    uart.state = NX_DEV_STATE_UNINITIALIZED;
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t uart_suspend(nx_lifecycle_t* self) {
    if (uart.state != NX_DEV_STATE_RUNNING) { return NX_ERR_INVALID_STATE; }
    nx_status_t status = uart_deinit(self);
    if (status == NX_OK) { uart.state = NX_DEV_STATE_SUSPENDED; }
    return status;
}
static nx_status_t uart_resume(nx_lifecycle_t* self) {
    return uart.state == NX_DEV_STATE_SUSPENDED ? uart_init(self) : NX_ERR_INVALID_STATE;
}
static nx_device_state_t uart_state(nx_lifecycle_t* self) { (void)self; return uart.state; }

static nx_status_t uart_submit(nx_uart_operations_t* self, const uint8_t* data,
                               size_t length, uint32_t timeout_ms, nx_uart_ticket_t* ticket) {
    (void)self;
    if (!ticket) { return NX_ERR_NULL_PTR; }
    ticket->sequence = 0;
    if (!data || !length || timeout_ms > INT32_MAX) {
        return NX_ERR_INVALID_PARAM;
    }
    if (nx_arch_in_isr() || uart.state != NX_DEV_STATE_RUNNING) { return NX_ERR_INVALID_STATE; }
    if (!timeout_ms) { return NX_ERR_TIMEOUT; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (uart.active) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    if (uart.sequence == UINT64_MAX) { nx_arch_irq_restore(saved); return NX_ERR_NO_RESOURCE; }
    uart.tx = data;
    uart.tx_length = length;
    uart.tx_position = 0;
    uart.start_ms = nx_gd32f470_millis();
    uart.timeout_ms = timeout_ms;
    uart.result = (nx_uart_result_t){ .status = NX_ERR_BUSY };
    ticket->sequence = ++uart.sequence;
    uart.active = true;
    usart_flag_clear(USART0, USART_FLAG_TC);
    nx_gd32_board_rs485_de(true);
    usart_interrupt_enable(USART0, USART_INT_TBE);
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t uart_poll(nx_uart_operations_t* self, nx_uart_ticket_t ticket,
                             nx_uart_result_t* result) {
    (void)self;
    if (!result || !ticket.sequence) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (ticket.sequence != uart.sequence) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (uart.active && (uint32_t)(nx_gd32f470_millis() - uart.start_ms) >= uart.timeout_ms) {
        abort_transfer(NX_ERR_TIMEOUT);
    }
    *result = uart.result;
    if (uart.active) { result->transferred = uart.tx_position; }
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t uart_cancel(nx_uart_operations_t* self, nx_uart_ticket_t ticket) {
    (void)self;
    if (!ticket.sequence) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (ticket.sequence != uart.sequence) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (uart.active) { abort_transfer(NX_ERR_CANCELLED); }
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t uart_receive_event(nx_uart_operations_t* self, nx_uart_rx_event_t* event) {
    (void)self;
    if (!event) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr() || uart.state != NX_DEV_STATE_RUNNING) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (uart.count) {
        *event = uart.rx[uart.head];
        uart.head = (uart.head + 1u) % NX_CONFIG_GD32_UART_RX_BUFFER_SIZE;
        --uart.count;
    } else if (uart.dropped) {
        *event = (nx_uart_rx_event_t){ .status = NX_ERR_FULL,
            .raw_error = uart.dropped, .timestamp_us = uart.overflow_timestamp,
            .resolution_us = 1u };
        uart.dropped = 0;
    } else { nx_arch_irq_restore(saved); return NX_ERR_NO_DATA; }
    nx_arch_irq_restore(saved);
    return NX_OK;
}

void USART0_IRQHandler(void) {
    uint32_t flags = USART_STAT0(USART0);
    const uint32_t errors = USART_STAT0_ORERR | USART_STAT0_NERR |
                            USART_STAT0_FERR | USART_STAT0_PERR;
    if (flags & (USART_STAT0_RBNE | errors)) {
        /* Reading STAT0 then DATA clears parity/framing/noise/overrun. */
        uint8_t value = (uint8_t)usart_data_receive(USART0);
        nx_uart_rx_event_t event = { .timestamp_us = nx_gd32f470_timestamp_us(),
            .resolution_us = 1u, .raw_error = flags & errors, .data = value,
            .has_data = (flags & USART_STAT0_RBNE) != 0u,
            .status = (flags & errors) ? NX_ERR_IO : NX_OK };
        enqueue_event(event);
    }
    if (!uart.active) { return; }
    if ((flags & USART_STAT0_TBE) && uart.tx_position < uart.tx_length) {
        usart_data_transmit(USART0, uart.tx[uart.tx_position++]);
        if (uart.tx_position == uart.tx_length) {
            usart_interrupt_disable(USART0, USART_INT_TBE);
            usart_interrupt_enable(USART0, USART_INT_TC);
        }
        /* Do not consume the TC value sampled before the final DATA write. */
        return;
    }
    if ((flags & USART_STAT0_TC) && uart.tx_position == uart.tx_length) {
        settle(NX_OK, true);
    }
}

static nx_status_t uart_send_sync(nx_tx_sync_t* self, const uint8_t* data,
                                  size_t length, uint32_t timeout_ms) {
    (void)self;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) { return NX_ERR_INVALID_STATE; }
    nx_uart_ticket_t ticket;
    nx_status_t status = uart_submit(&uart.operations, data, length, timeout_ms, &ticket);
    if (status != NX_OK) { return status; }
    nx_uart_result_t result;
    do {
        status = uart_poll(&uart.operations, ticket, &result);
        if (status != NX_OK) { return status; }
    } while (!result.settled);
    return result.status;
}
static nx_status_t uart_receive_legacy(nx_rx_async_t* self, uint8_t* data, size_t* length) {
    (void)self;
    if (!data || !length || !*length) { return NX_ERR_INVALID_PARAM; }
    size_t capacity = *length;
    *length = 0;
    while (*length < capacity) {
        nx_uart_rx_event_t event;
        nx_status_t status = uart_receive_event(&uart.operations, &event);
        if (status == NX_ERR_NO_DATA) { break; }
        if (status != NX_OK) { return status; }
        if (event.status != NX_OK) { return event.status; }
        if (event.has_data) { data[(*length)++] = event.data; }
    }
    return *length ? NX_OK : NX_ERR_NO_DATA;
}
static nx_uart_operations_t* get_operations(nx_uart_t* self) { (void)self; return &uart.operations; }
static nx_tx_async_t* get_tx_async(nx_uart_t* self) { (void)self; return NULL; }
static nx_rx_async_t* get_rx_async(nx_uart_t* self) { (void)self; return &uart.rx_async; }
static nx_tx_sync_t* get_tx_sync(nx_uart_t* self) { (void)self; return &uart.tx_sync; }
static nx_rx_sync_t* get_rx_sync(nx_uart_t* self) { (void)self; return NULL; }
static nx_lifecycle_t* get_lifecycle(nx_uart_t* self) { (void)self; return &uart.lifecycle; }
static nx_power_t* get_power(nx_uart_t* self) { (void)self; return NULL; }
static nx_status_t construct_uart(const nx_device_t* descriptor, void** out) {
    if (!out) { return NX_ERR_NULL_PTR; }
    *out = NULL;
    if (!descriptor || descriptor->state != &device_state) { return NX_ERR_INVALID_PARAM; }
    uart.api = (nx_uart_t){ .get_operations = get_operations, .get_tx_async = get_tx_async,
        .get_rx_async = get_rx_async, .get_tx_sync = get_tx_sync,
        .get_rx_sync = get_rx_sync, .get_lifecycle = get_lifecycle, .get_power = get_power };
    uart.operations = (nx_uart_operations_t){ .submit = uart_submit, .poll = uart_poll,
        .cancel = uart_cancel, .receive_event = uart_receive_event };
    uart.lifecycle = (nx_lifecycle_t){ .init = uart_init, .deinit = uart_deinit,
        .suspend = uart_suspend, .resume = uart_resume, .get_state = uart_state };
    uart.tx_sync.send = uart_send_sync;
    uart.rx_async.receive = uart_receive_legacy;
    *out = &uart.api;
    return NX_OK;
}
NX_DEVICE_REGISTER_TYPED(NX_UART, 0, "UART0", NULL, &device_state, NX_DEVICE_CLASS_UART,
    NX_DEVICE_CAP_UART_OPERATIONS | NX_DEVICE_CAP_UART_CANCEL | NX_DEVICE_CAP_UART_RX_EVENTS, construct_uart, NULL);
