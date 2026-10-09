#include "hal/provider/nx_device_provider.h"
#include "log/log.h"
#include "log/log_uart.h"
#include "osal/osal.h"
#include "fixtures/typed_uart_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef LOG_TEST_WRAP_ALLOC
static unsigned allocations;
void* __real_malloc(size_t size);
void* __wrap_malloc(size_t size) { ++allocations; return __real_malloc(size); }
#endif
static void test_bounded_copy_and_wire_idle(void) {
    typed_uart_port_reset();
    log_uart_backend_t sink = {0};
#ifdef LOG_TEST_WRAP_ALLOC
    allocations = 0;
#endif
    assert(log_backend_uart_init(&sink, "UART_TEST", 100, 20) == LOG_OK);
#ifdef LOG_TEST_WRAP_ALLOC
    assert(allocations == 0);
#endif
    assert(log_init(NULL) == LOG_OK && log_backend_register(&sink.backend) == LOG_OK);
    char msg[] = "owned copy";
    for (unsigned i = 0; i < LOG_UART_QUEUE_DEPTH; ++i) assert(log_write_raw(msg, 10) == LOG_OK);
    assert(log_write_raw(msg, 10) == LOG_ERROR_FULL);
    memset(msg, 'x', 10);
    assert(log_backend_uart_service(&sink) == LOG_OK);
    assert(uart_port.borrowed && memcmp(uart_port.borrowed, "owned copy", 10) == 0);
    assert(log_backend_uart_shutdown(&sink, true) == LOG_ERROR_BUSY); /* Registered. */
    uart_port.complete = true; uart_port.wire_idle = false;
    assert(log_backend_uart_service(&sink) == LOG_OK);
    log_uart_stats_t stats;
    assert(log_backend_uart_get_stats(&sink, &stats) == LOG_OK);
    assert(stats.pending == LOG_UART_QUEUE_DEPTH && stats.completed == 0);
    uart_port.wire_idle = true;
    assert(log_backend_uart_service(&sink) == LOG_OK);
    uart_port.auto_complete = true;
    assert(log_backend_uart_flush(&sink) == LOG_OK);
    assert(log_backend_uart_get_stats(&sink, &stats) == LOG_OK);
    assert(stats.accepted == LOG_UART_QUEUE_DEPTH && stats.completed == LOG_UART_QUEUE_DEPTH && stats.dropped == 1);
    assert(log_backend_unregister("uart") == LOG_OK && !sink.opened);
    assert(log_deinit() == LOG_OK && log_backend_uart_shutdown(&sink, true) == LOG_OK);
}
static void test_failed_cancel_and_close_retains_owner(void) {
    typed_uart_port_reset(); log_uart_backend_t sink = {0};
    assert(log_backend_uart_init(&sink, "UART_TEST", 100, 5) == LOG_OK);
    nx_device_ref_t stale = sink.device;
    assert(sink.backend.write(sink.backend.ctx, "leased", 6) == LOG_OK);
    assert(log_backend_uart_service(&sink) == LOG_OK);
    const uint8_t* borrowed = uart_port.borrowed;
    assert(log_backend_uart_flush(&sink) == LOG_ERROR_TIMEOUT);
    uart_port.cancel_status = NX_ERR_HARDWARE;
    assert(log_backend_uart_shutdown(&sink, true) == LOG_ERROR_BACKEND);
    assert(sink.opened && sink.active && uart_port.borrowed == borrowed && memcmp(borrowed, "leased", 6) == 0);
    uart_port.cancel_status = NX_OK; uart_port.close_status = NX_ERR_BUSY;
    assert(log_backend_uart_shutdown(&sink, true) == LOG_ERROR_BUSY && sink.opened);
    uart_port.close_status = NX_OK;
    assert(log_backend_uart_shutdown(&sink, true) == LOG_OK);
    nx_device_caps_t caps; assert(nx_device_query(stale, &caps) == NX_ERR_INVALID_STATE);
    assert(log_backend_uart_shutdown(&sink, true) == LOG_OK);
}
static void test_unknown_ticket_recovery(void) {
    typed_uart_port_reset(); log_uart_backend_t sink = {0};
    assert(log_backend_uart_init(&sink, "UART_TEST", 100, 5) == LOG_OK);
    assert(sink.backend.write(sink.backend.ctx, "quarantined", 11) == LOG_OK);
    uart_port.zero_ticket = true;
    assert(log_backend_uart_service(&sink) == LOG_ERROR_BACKEND && sink.unknown_lease);
    uart_port.close_status = NX_ERR_HARDWARE;
    assert(log_backend_uart_shutdown(&sink, true) == LOG_ERROR_BACKEND && sink.opened && uart_port.borrowed);
    uart_port.close_status = NX_OK;
    assert(log_backend_uart_shutdown(&sink, true) == LOG_OK && !uart_port.borrowed);
}
static void test_sticky_failed_delivery(void) {
    typed_uart_port_reset(); log_uart_backend_t sink = {0};
    assert(log_backend_uart_init(&sink, "UART_TEST", 100, 5) == LOG_OK);
    assert(sink.backend.write(sink.backend.ctx, "failure", 7) == LOG_OK);
    uart_port.auto_complete = true; uart_port.terminal_status = NX_ERR_IO;
    assert(log_backend_uart_flush(&sink) == LOG_ERROR_BACKEND);
    assert(log_backend_uart_flush(&sink) == LOG_ERROR_BACKEND); /* Never masks a lost accepted entry. */
    assert(log_backend_uart_shutdown(&sink, true) == LOG_OK);
}
static void test_static_backends(void) {
#if LOG_USE_STATIC_ALLOC
#ifdef LOG_TEST_WRAP_ALLOC
    allocations = 0;
#endif
    log_backend_t* console = log_backend_console_create();
    log_backend_t* memory = log_backend_memory_create(128);
    assert(console && memory);
    assert(log_backend_console_create() == NULL && log_backend_memory_create(128) == NULL);
    assert(log_backend_console_destroy(console) == LOG_OK && log_backend_memory_destroy(memory) == LOG_OK);
#ifdef LOG_TEST_WRAP_ALLOC
    assert(allocations == 0);
#endif
#endif
}
int main(void) {
    assert(osal_init() == OSAL_OK);
    test_bounded_copy_and_wire_idle(); test_failed_cancel_and_close_retains_owner();
    test_unknown_ticket_recovery(); test_sticky_failed_delivery(); test_static_backends();
    assert(nx_device_registry_reset() == NX_OK);
    osal_stats_t stats; assert(osal_get_stats(&stats) == OSAL_OK && stats.mem_alloc_count == 0 && stats.mutex_count == 0);
    printf("5 typed UART logging contract groups passed (static=%d)\n", LOG_USE_STATIC_ALLOC);
    return 0;
}
