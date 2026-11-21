/**
 * \file            log_uart.h
 * \brief           Caller-owned UART record with settled lifetime
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LOG_UART_H
#define NEXUS_COMPONENTS_LOG_UART_H
#include "nexus/components/log.h"
#include "nexus/io/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Explicit one-record staging, with no default queue or
 * worker.
 *
 * \note            One task executor owns all sink, service and stop
 * calls and exclusively owns UART TX. RX remains with its declared owner.
 *                  Buffer/adapter/port survive through SETTLED even on timeout
 * or failed cancellation. SUCCESS from write means admission of a private copy,
 * not physical completion. Statistics saturate.
 */
typedef struct {
    const nx_uart_port_t* uart;
    uint8_t* buffer;
    size_t capacity;
    nx_uart_tx_request_t request;
    uint32_t accepted;
    uint32_t completed;
    uint32_t dropped;
    nx_result_t last_result;
    size_t last_transferred;
    bool pending;
    bool stopped;
    bool cancel_requested;
} nx_log_uart_t;

/**
 * \brief           Bind one unused staging record to an existing UART owner.
 *
 * \param[out]      sink: Unpublished caller-owned adapter.
 *
 * \param[in]       uart: Existing live UART face, exclusively owned for TX.
 *
 * \param[out]      buffer: Positive caller-owned staging span, disjoint from
 * sink.
 *
 * \param[in]       capacity: Staging bytes; each record must fit
 * completely.
 *
 * \return          SUCCESS or INVALID without modifying rejected
 * storage.
 *
 * \note            Task-only initialization; no device initialization
 * or I/O.
 */
nx_result_t nx_log_uart_init(nx_log_uart_t* sink, const nx_uart_port_t* uart,
                             void* buffer, size_t capacity);

/**
 * \brief           Obtain the existing log sink interface over this adapter.
 *
 * \param[in,out]   sink: Initialized adapter kept alive through drain.
 *
 * \return          Synchronous caller-byte interface; NULL produces empty port.
 *
 * \note            write copies before returning; no input pointer is retained.
 *                  flush services once and returns BUSY until SETTLED,
 * preserving its actual final result. No wait, retry or timeout revocation.
 */
nx_log_sink_port_t nx_log_uart_sink(nx_log_uart_t* sink);

/**
 * \brief           Advance the actual UART once and observe final settlement.
 *
 * \param[in,out]   sink: Same unique task executor as write/flush/stop.
 *
 * \return          BUSY while borrowed; otherwise final transfer result or
 * SUCCESS when idle. A short successful transfer is reported as IO.
 *
 * \note No
 * staging reuse before acquire-observed SETTLED. This call does not stop shared
 * UART RX or release hardware ownership.
 */
nx_result_t nx_log_uart_service(nx_log_uart_t* sink);

/**
 * \brief           Close admission and request cancellation without reclaiming.
 *
 * \param[in,out]   sink: Unique executor; future log publishers are joining.
 *
 * \return          SUCCESS when idle, BUSY while drain remains, or cancel
 * error.
 *
 * \note            Continue service after BUSY or failed drain. Adapter
 * and staging storage remain live through settlement and publisher join. This
 * does not call nx_uart_port_stop.
 */
nx_result_t nx_log_uart_stop(nx_log_uart_t* sink);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_LOG_UART_H */
