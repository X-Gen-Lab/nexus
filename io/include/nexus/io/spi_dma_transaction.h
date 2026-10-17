/**
 * \file            spi_dma_transaction.h
 * \brief           Optional finite whole-CS segmented SPI DMA request contract
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_SPI_DMA_TRANSACTION_H
#define NEXUS_IO_SPI_DMA_TRANSACTION_H
#include "nexus/io/spi.h"
#include "nexus/io/spi_transaction.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Explicit DMA words with caller-owned dummy/discard cells.
 * \note            Both addresses are mandatory. A FIXED direction borrows one
 *                  word; an incrementing direction borrows words elements.
 *                  Width is common to all segments. DMA never uses the polling
 *                  segment's implicit NULL/dummy representation or scratch.
 */
typedef struct {
    const void* tx;
    void* rx;
    size_t words;
    uint8_t memory_flags;
} nx_spi_dma_segment_t;

/**
 * \brief           Caller-owned request retaining every segment through drain.
 * \note            Initialize base before prepare. ACCEPTED borrows this
 * object, its descriptor array, all TX/RX spans and fixed cells until
 *                  acquire-observed SETTLED. Intermediate segment completion,
 *                  cancellation and timeout never release any of these loans.
 *                  Completion counts words of successfully settled segments;
 *                  partial effects in a failed segment remain unquantified.
 */
typedef struct {
    nx_request_t base;
    const nx_spi_dma_segment_t* segments;
    size_t count;
    uint8_t word_bits;
} nx_spi_dma_transaction_request_t;

typedef struct {
    nx_result_t (*submit)(void* context,
                          nx_spi_dma_transaction_request_t* request);
} nx_spi_dma_transaction_endpoint_ops_t;

typedef struct {
    const nx_spi_dma_transaction_endpoint_ops_t* ops;
    void* context;
} nx_spi_dma_transaction_endpoint_t;

typedef struct {
    nx_result_t (*cancel)(void* context,
                          nx_spi_dma_transaction_request_t* request);
    void (*service)(void* context);
    nx_result_t (*stop)(void* context);
} nx_spi_dma_transaction_port_ops_t;

typedef struct {
    const nx_spi_dma_transaction_port_ops_t* ops;
    void* context;
} nx_spi_dma_transaction_port_t;

/**
 * \brief           Prepare unborrowed storage after validating every span.
 * \param[in,out]   request: Initialized READY or consumed SETTLED storage.
 * \param[in]       segments: Immutable caller-owned array, at most
 * MAX_SEGMENTS. \param[in]       count: Positive segment count; every words
 * count is positive. \param[in]       word_bits: Common hardware width of 8 or
 * 16 bits. \param[in]       deadline: Absolute monotonic deadline for the
 * complete chain. \return          SUCCESS, INVALID/UNSUPPORTED/STATE; no
 * borrow on failure. \note            Descriptors and payload never overlap
 * request storage. RX cannot overwrite any TX or descriptor. Repeated RX alias
 * is allowed only for the exact same explicit FIXED discard cell.
 *                  Provider-specific domains are checked at admission.
 */
nx_result_t
nx_spi_dma_transaction_prepare(nx_spi_dma_transaction_request_t* request,
                               const nx_spi_dma_segment_t* segments,
                               size_t count, uint8_t word_bits,
                               nx_time_us_t deadline);
/** \brief Validate immutable fields without admission or memory modification.
 */
nx_result_t nx_spi_dma_transaction_validate(
    const nx_spi_dma_transaction_request_t* request);
/**
 * \brief           Admit one finite chain on a task-only single executor.
 * \param[in]       endpoint: Stable enhanced DMA endpoint and provider state.
 * \param[in,out]   request: READY descriptor, retained only on SUCCESS.
 * \return          SUCCESS means ACCEPTED, possibly already SETTLED. Rejection
 *                  retains no references and creates no hardware effects.
 * \note            All domains and segments are validated before admission.
 *                  One CS interval spans the chain; SCK gaps between segments
 *                  are permitted. DMA TC alone never establishes wire idle.
 */
nx_result_t
nx_spi_dma_transaction_submit(const nx_spi_dma_transaction_endpoint_t* endpoint,
                              nx_spi_dma_transaction_request_t* request);
/**
 * \brief           Cancel an exact live request while retaining all its loans.
 * \param[in]       port: Same single executor used by submit and service.
 * \param[in,out]   request: Exact current request or acquire-observed SETTLED.
 * \return          SUCCESS starts/observes cancellation; INVALID/STATE/CONTEXT
 *                  rejects without revoking any existing loan.
 */
nx_result_t
nx_spi_dma_transaction_cancel(const nx_spi_dma_transaction_port_t* port,
                              nx_spi_dma_transaction_request_t* request);
/**
 * \brief           Advance a bounded phase boundary, completion or drain.
 * \param[in]       port: Task-only enhanced controller; continue during stop.
 * \note            No wait, allocation, payload copy, hidden task or OS lock.
 *                  Failed drain/release retains QUARANTINED storage. SETTLED
 *                  follows final hardware detach and CS release and is the
 *                  provider's final access to caller request and descriptors.
 */
void nx_spi_dma_transaction_service(const nx_spi_dma_transaction_port_t* port);
/**
 * \brief           Close admission and stop after complete source withdrawal.
 * \param[in]       port: Stable single-executor state, producers quiesced.
 * \return          SUCCESS proves stop; BUSY needs further service and retains
 *                  all outstanding storage, including quarantined requests.
 * \note            Repeated successful stop never touches hardware again;
 *                  another owner may already have acquired the controller.
 */
nx_result_t
nx_spi_dma_transaction_stop(const nx_spi_dma_transaction_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
