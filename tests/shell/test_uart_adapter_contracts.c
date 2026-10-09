#include "hal/provider/nx_device_provider.h"
#include "shell/shell_uart.h"
#include "osal/osal.h"
#include "typed_uart_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void test_close_failure_is_retryable(void) {
    typed_uart_port_reset();
    assert(shell_uart_backend_init("UART_TEST") == SHELL_OK);
    nx_device_ref_t stale = {.descriptor = &uart_port.descriptor, .owner = uart_port.state.owner,
        .generation = uart_port.state.generation, .device_class = NX_DEVICE_CLASS_UART};
    uart_port.close_status = NX_ERR_BUSY;
    assert(shell_uart_backend_deinit() == SHELL_ERROR_BUSY && shell_uart_backend_is_initialized());
    assert(shell_uart_backend_init("UART_TEST") == SHELL_ERROR_ALREADY_INIT);
    uart_port.close_status = NX_OK;
    assert(shell_uart_backend_deinit() == SHELL_OK && !shell_uart_backend_is_initialized());
    nx_device_caps_t caps; assert(nx_device_query(stale, &caps) == NX_ERR_INVALID_STATE);
    assert(shell_uart_backend_deinit() == SHELL_OK);
}
static void test_failed_cancel_holds_static_tx_copy(void) {
    typed_uart_port_reset(); assert(shell_uart_backend_init("UART_TEST") == SHELL_OK);
    uart_port.poll_status = NX_ERR_IO; uart_port.cancel_status = NX_ERR_HARDWARE;
    uint8_t message[] = "persistent";
    assert(shell_uart_backend.write(message, 10) == 0 && uart_port.borrowed);
    memset(message, 'x', 10);
    assert(memcmp(uart_port.borrowed, "persistent", 10) == 0);
    assert(shell_uart_backend_deinit() == SHELL_ERROR && shell_uart_backend_is_initialized());
    assert(shell_uart_backend.write(message, 10) == 0);
    uart_port.cancel_status = NX_OK; uart_port.close_status = NX_ERR_BUSY;
    assert(shell_uart_backend_deinit() == SHELL_ERROR_BUSY && shell_uart_backend_is_initialized());
    uart_port.close_status = NX_OK;
    assert(shell_uart_backend_deinit() == SHELL_OK);
}
static void test_unknown_ticket_and_rx_fault(void) {
    typed_uart_port_reset(); assert(shell_uart_backend_init("UART_TEST") == SHELL_OK);
    uint8_t byte;
    assert(shell_uart_backend.read(&byte, 1) == 0);
    uart_port.rx_error = true;
    assert(shell_uart_backend.read(&byte, 1) < 0 && shell_uart_backend_last_status() == NX_ERR_PARITY);
    uart_port.zero_ticket = true;
    assert(shell_uart_backend.write((const uint8_t*)"unknown", 7) == 0 && uart_port.borrowed);
    uart_port.close_status = NX_ERR_HARDWARE;
    assert(shell_uart_backend_deinit() == SHELL_ERROR && shell_uart_backend_is_initialized());
    uart_port.close_status = NX_OK;
    assert(shell_uart_backend_deinit() == SHELL_OK && !uart_port.borrowed);
}
static void test_copy_chunks_and_required_capability(void) {
    typed_uart_port_reset(); uart_port.uart.get_operations = NULL;
    assert(shell_uart_backend_init("UART_TEST") == SHELL_ERROR_UNSUPPORTED && !shell_uart_backend_is_initialized());
    typed_uart_port_reset(); uart_port.auto_complete = true;
    assert(shell_uart_backend_init("UART_TEST") == SHELL_OK);
    uint8_t msg[600]; memset(msg, 'a', sizeof(msg));
    assert(shell_uart_backend.write(msg, sizeof(msg)) == sizeof(msg) && uart_port.submits == 3);
    assert(shell_uart_backend_deinit() == SHELL_OK);
}
int main(void) {
    assert(osal_init() == OSAL_OK);
    test_close_failure_is_retryable(); test_failed_cancel_holds_static_tx_copy();
    test_unknown_ticket_and_rx_fault(); test_copy_chunks_and_required_capability();
    assert(nx_device_registry_reset() == NX_OK);
    puts("4 typed Shell UART teardown/buffer contract groups passed"); return 0;
}
