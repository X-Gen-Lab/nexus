/** Caller-owned typed UART sink. SPDX-License-Identifier: MIT */
#ifndef LOG_UART_H
#define LOG_UART_H
#include "log_backend.h"
#include "hal/base/nx_device.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifndef LOG_UART_QUEUE_DEPTH
#define LOG_UART_QUEUE_DEPTH 4u
#endif
#ifndef LOG_UART_MESSAGE_SIZE
#define LOG_UART_MESSAGE_SIZE ((size_t)LOG_MAX_MSG_LEN * 2u)
#endif
typedef struct {
    size_t accepted, completed, dropped, failed, pending;
    nx_status_t last_status;
    bool buffer_leased;
} log_uart_stats_t;
/** Initialize with {0}. Storage must outlive registration, all service calls,
 * and any failed shutdown. No heap allocation in either log allocation mode. */
typedef struct {
    log_backend_t backend;
    nx_device_ref_t device;
    nx_uart_ticket_t ticket;
    uint8_t messages[LOG_UART_QUEUE_DEPTH][LOG_UART_MESSAGE_SIZE];
    size_t lengths[LOG_UART_QUEUE_DEPTH];
    size_t head, count;
    uint32_t tx_timeout_ms, flush_timeout_ms;
    log_uart_stats_t stats;
    bool opened, closing, servicing, active, unknown_lease;
} log_uart_backend_t;
/** Task-only, owns an exclusive typed device reference. Budgets are finite,
 * nonzero and <= INT32_MAX. No fallback to legacy synchronous UART. */
log_status_t log_backend_uart_init(log_uart_backend_t* sink, const char* name,
                                   uint32_t tx_timeout_ms, uint32_t flush_timeout_ms);
/** No-wait pump. Caller schedules it explicitly; no hidden task/timer exists.
 * An accepted write copies into bounded storage and can return FULL. */
log_status_t log_backend_uart_service(log_uart_backend_t* sink);
/** Flush accepted messages, including electrical wire idle, within the budget.
 * timeout/error retains the reference and any buffer lease for a retry. */
log_status_t log_backend_uart_flush(log_uart_backend_t* sink);
/** Unregister and stop independent service users first. discard=false drains;
 * discard=true cancels and reports discarded count. Failure retains storage. */
log_status_t log_backend_uart_shutdown(log_uart_backend_t* sink, bool discard);
log_status_t log_backend_uart_get_stats(log_uart_backend_t* sink, log_uart_stats_t* out);
#ifdef __cplusplus
}
#endif
#endif
