/**
 * \file            spi_owner.h
 * \brief           Explicit polling SPI executor for the bounded owner adapter
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SPI_OWNER_H
#define NEXUS_COMPONENTS_SPI_OWNER_H
#include "nexus/components/bus_owner.h"
#include "nexus/io/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Caller-owned SPI transaction descriptor, borrowed until SETTLED. */
typedef struct {
    const nx_spi_endpoint_t* endpoint;
    const uint8_t* tx;
    uint8_t* rx;
    size_t length;
} nx_spi_owner_operation_t;

/** \brief Exact polling executor storage; no payload copy or queue. */
typedef struct {
    const nx_spi_port_t* port;
    nx_result_t result;
    size_t transferred;
    bool pending;
} nx_spi_owner_executor_t;

/**
 * \brief           Bind an explicit synchronous SPI execution adapter
 * \param[out]      executor: Fresh storage with no outstanding operation
 * \param[in]       port: Controller exclusively serviced by this owner
 * \return          Port for nx_bus_owner_init, or a zero port for invalid input
 * \note            start executes a real CPU-active polling transaction and
 *                  releases all IO buffers before returning. Owner service
 *                  later publishes its caller-owned request. Polling cannot be
 *                  preempted by another task's cancel; once the polling call
 *                  returns, its result is final despite later cancellation or
 *                  publication delay. Do not claim IRQ/DMA mode or low-CPU
 *                  asynchronous hardware execution. Endpoints must prove their
 *                  controller identity before any wire effect; distinct chip
 *                  selects on this controller remain valid. The controller and
 *                  executor outlive owner shutdown and all observers.
 */
nx_owner_executor_port_t
nx_spi_owner_executor_port(nx_spi_owner_executor_t* executor,
                           const nx_spi_port_t* port);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_SPI_OWNER_H */
