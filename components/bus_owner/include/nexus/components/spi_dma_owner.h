/**
 * \file            spi_dma_owner.h
 * \brief           Optional typed whole-CS SPI executor for bounded BusOwner
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SPI_DMA_OWNER_H
#define NEXUS_COMPONENTS_SPI_DMA_OWNER_H
#include "nexus/components/bus_owner.h"
#include "nexus/io/spi_dma_owner_binding.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Caller-owned queued and active SPI chain.
 * \note            The outer deadline includes queue residence. The chain,
 *                  descriptors and all TX/RX or fixed cells stay borrowed until
 *                  acquire-observed outer SETTLED. Counts and results use
 * words.
 */
typedef struct {
    const nx_spi_dma_transaction_endpoint_t* endpoint;
    const nx_spi_dma_segment_t* segments;
    size_t count;
    uint8_t word_bits;
} nx_spi_dma_owner_operation_t;
/** \brief Selected-only exact metadata; no queue, payload copy or worker. */
typedef struct {
    const nx_spi_dma_owner_binding_t* binding;
    nx_spi_dma_transaction_request_t inner;
    bool pending;
} nx_spi_dma_owner_executor_t;
/**
 * \brief           Bind a fresh executor without opening hardware or admission.
 * \param[in,out]   executor: Zero fresh storage, distinct from capability/face.
 * \param[in]       binding: Initialized bus exclusively serviced by this owner.
 * \return          Executor for nx_bus_owner_init, zero port on invalid input.
 * \note            Task-only, all lifecycle users joined before replacement.
 *                  Uses original FIFO/slots/epochs; final provider terminal
 *                  result survives late cancellation. Continue unique executor
 *                  service during stop and quarantine. Hardware stop, publisher
 *                  join and observer join remain explicit external steps.
 */
nx_owner_executor_port_t
nx_spi_dma_owner_executor_port(nx_spi_dma_owner_executor_t* executor,
                               const nx_spi_dma_owner_binding_t* binding);
/**
 * \brief           Check typed storage, then use bounded owner admission.
 * \param[in,out]   owner: Owner with this exact typed executor.
 * \param[in,out]   request: Prepared READY outer request.
 * \param[in]       operation: Immutable chain through outer SETTLED.
 * \param[in]       completion: Optional static caller hint, or NULL.
 * \param[out]      ticket: Stable slot/epoch identity on SUCCESS.
 * \return          SUCCESS means ACCEPTED; failure retains nothing.
 * \note            The request has an absolute deadline including queue time.
 *                  Task multi-producer checks run outside the metadata guard.
 *                  There is no capacity scan, allocation, OS lock or worker.
 *                  One operation has one CS interval with allowed SCK gaps.
 *                  Schedule bounded service even when hints fail. Waiters never
 *                  service the shared controller. Queued provider failures
 *                  settle later without converting admission into rejection.
 */
nx_result_t nx_spi_dma_owner_submit(nx_bus_owner_t* owner,
                                    nx_request_t* request,
                                    nx_spi_dma_owner_operation_t* operation,
                                    const nx_wait_port_t* completion,
                                    nx_owner_ticket_t* ticket);
#ifdef __cplusplus
}
#endif
#endif
