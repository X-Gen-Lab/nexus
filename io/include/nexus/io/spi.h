/**
 * \file            spi.h
 *
 * \brief           Single-executor polling SPI endpoints and complete CS
 *                  transactions.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_SPI_H
#define NEXUS_SPI_H

#include "nexus/core/request.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_spi_port nx_spi_port_t;
typedef struct nx_spi_endpoint nx_spi_endpoint_t;

/**
 * \brief           Caller-owned finite SPI request with one whole-CS interval.
 *
 * \note            Initialize base before prepare. Accepted requests borrow
 *                  both non-NULL buffers until acquire-observed SETTLED,
 *                  including cancellation and timeout drain. QUARANTINED
 *                  retains the loan.
 */
typedef struct {
    nx_request_t base;
    const uint8_t* tx;
    uint8_t* rx;
    size_t length;
} nx_spi_request_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*recover)(void* context);
    nx_result_t (*cancel)(void* context, nx_spi_request_t* request);
    void (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*attach_wake)(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling);
} nx_spi_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_spi_port {
    const nx_spi_ops_t* ops;
    void* context;
};

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*transfer)(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred);
    nx_result_t (*submit)(void* context, nx_spi_request_t* request);
    nx_result_t (*start_admitted)(void* context, nx_spi_request_t* request);
    bool (*on_port)(const void* context, const nx_spi_port_t* port);
} nx_spi_endpoint_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_spi_endpoint {
    const nx_spi_endpoint_ops_t* ops;
    void* context;
};
/**
 * \brief           Execute one bounded full-duplex 8-bit polling transaction.
 *
 * \param[in]       endpoint: Fixed CS/mode/rate binding on one controller.
 *
 * \param[in]       tx: Transmit bytes, or NULL to send 0xff.
 *
 * \param[out]      rx: Receive bytes, or NULL to discard.
 *
 * \param[in]       length: Positive transfer length; at least one buffer
 *                  exists.
 *
 * \param[in]       deadline: Absolute deadline; includes adapter queue
 *                  residence.
 *
 * \param[out]      transferred: Complete bytes; reset to zero before execution.
 *
 * \return          Success, BUSY/INVALID/TIMEOUT/IO. Return releases all
 *                  buffers and CS after drain; polling is CPU-active, not
 *                  IRQ/DMA async.
 *
 * \note            Task-only single executor, no allocation/OS lock. One CS
 *                  interval covers the entire transfer. Deadline cannot preempt
 *                  an in-flight hardware byte; peripheral drain may exceed it.
 */
nx_result_t nx_spi_endpoint_transfer(const nx_spi_endpoint_t* endpoint,
                                     const uint8_t* tx, uint8_t* rx,
                                     size_t length, nx_time_us_t deadline,
                                     size_t* transferred);
/**
 * \brief           Recover an idle SPI controller after its retained fault.
 *
 * \param[in]       port: Single executor with no live transaction or CS borrow.
 *
 * \return          Success only after reset/idle proof; BUSY/STATE/UNSUPPORTED
 *                  preserves recovery responsibility and never replays work.
 */
nx_result_t nx_spi_port_recover(const nx_spi_port_t* port);
/**
 * \brief           Prepare an unborrowed finite request without copying
 *                  buffers.
 *
 * \param[in,out]   request: Fresh initialized or consumed SETTLED descriptor.
 *
 * \param[in]       tx: Nonempty TX bytes or NULL when the selected mode
 *                  supports dummy transmission; immutable through final
 *                  settlement.
 *
 * \param[out]      rx: RX buffer or NULL when the selected mode supports
 *                  discard.
 *
 * \param[in]       length: Positive byte count with at least one supplied
 *                  buffer.
 *
 * \param[in]       deadline: Absolute monotonic deadline including queue time.
 *
 * \return          Success or INVALID/STATE; failure creates no buffer loan.
 */
nx_result_t nx_spi_request_prepare(nx_spi_request_t* request, const uint8_t* tx,
                                   uint8_t* rx, size_t length,
                                   nx_time_us_t deadline);
/**
 * \brief           Submit one finite asynchronous whole-CS transaction.
 *
 * \param[in]       endpoint: Initialized endpoint on a single-executor bus.
 *
 * \param[in,out]   request: READY storage; retained only after NX_SUCCESS.
 *
 * \return          Success means ACCEPTED; all rejection errors create zero
 *                  retained references and zero residual hardware effects.
 *
 * \note            Task-only bounded admission. Selected modes declare DMA
 *                  domain/length/buffer requirements. DMA TC alone never proves
 *                  peripheral idle; the bus executor continues service to
 *                  SETTLED.
 */
nx_result_t nx_spi_endpoint_submit(const nx_spi_endpoint_t* endpoint,
                                   nx_spi_request_t* request);
/**
 * \brief           Start an adapter-admitted QUEUED request without
 *                  readmission.
 *
 * \param[in]       endpoint: Initialized exact CS binding and bus executor.
 *
 * \param[in,out]   request: Live QUEUED storage already borrowed by its
 *                  adapter.
 *
 * \return          Success hands execution to the bus; BUSY retains QUEUED.
 *                  Other errors must be settled by the adapter executor.
 */
nx_result_t nx_spi_endpoint_start_admitted(const nx_spi_endpoint_t* endpoint,
                                           nx_spi_request_t* request);
/**
 * \brief           Verify an asynchronous endpoint belongs to the serviced bus.
 *
 * \param[in]       endpoint: Immutable endpoint identity kept alive by its
 *                  caller.
 *
 * \param[in]       port: Exact execution bus bound to an asynchronous adapter.
 *
 * \return          True only when the provider proves matching controller state
 *                  and compatible bus methods; false for invalid/missing
 *                  methods.
 *
 * \note            Task/IRQ bounded cold identity check, no lookup or ownership
 *                  effects. An adapter checks this before borrowing a request.
 */
bool nx_spi_endpoint_on_port(const nx_spi_endpoint_t* endpoint,
                             const nx_spi_port_t* port);
/**
 * \brief           Request cancellation without revoking DMA/CS buffer loans.
 *
 * \param[in]       port: Same single execution owner as submit and service.
 *
 * \param[in,out]   request: Exact live active request, never a stale raw
 *                  pointer.
 *
 * \return          Success starts drain or observes prior settlement; STATE for
 *                  another request, UNSUPPORTED for a polling-only provider.
 *
 * \note            Cross-task controls use stable adapter slots and epochs.
 */
nx_result_t nx_spi_port_cancel(const nx_spi_port_t* port,
                               nx_spi_request_t* request);
/**
 * \brief           Advance one asynchronous bus under its single task executor.
 *
 * \param[in]       port: Live provider; call while stopping and throughout
 *                  drain.
 *
 * \note            Bounded no-wait service. Final publication follows DMA/IRQ
 *                  detach and peripheral-idle proof; failed drain quarantines.
 */
void nx_spi_port_service(const nx_spi_port_t* port);
/**
 * \brief           Close admission and retain active sources until full drain.
 *
 * \param[in]       port: Live bus; all external producers already quiesced.
 *
 * \return          Success proves source release, BUSY requires service, or
 *                  UNSUPPORTED when the provider has no asynchronous lifecycle.
 */
nx_result_t nx_spi_port_stop(const nx_spi_port_t* port);
/**
 * \brief           Attach an optional hint sink to asynchronous bus IRQ
 *                  publishers.
 *
 * \param[in]       port: One initialized bus controlled by a task executor.
 *
 * \param[in]       wake: Immutable borrowed target or NULL to detach.
 *
 * \param[in]       syscall_ceiling: Optional additional unshifted kernel-safe
 *                  priority floor; cannot weaken generated policy. PRIMASK
 *                  kernel targets require zero. Ignored without kernel calls.
 *
 * \return          Success, PERMISSION for unsafe actual priority,
 *                  CONTEXT/STATE for illegal control or UNSUPPORTED for a
 *                  polling-only bus.
 *
 * \note            Join old publishers and waiters before freeing wake storage.
 */
nx_result_t nx_spi_port_attach_wake(const nx_spi_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif

#endif
