/**
 * \file            spi_async_owner.h
 * \brief           Optional finite async SPI executor for one explicit bus
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SPI_ASYNC_OWNER_H
#define NEXUS_COMPONENTS_SPI_ASYNC_OWNER_H

#include "nexus/components/spi_owner.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Exact async storage independent of the polling adapter.
 * \note            inner is a private publication object. Its borrow must end
 *                  before the outer owner publishes SETTLED. No queue, payload
 *                  copy, allocation or hidden worker is created.
 */
typedef struct {
    const nx_spi_port_t* port;
    nx_spi_request_t inner;
    bool pending;
} nx_spi_async_owner_executor_t;

/**
 * \brief           Bind one async-capable bus to an explicit owner executor.
 * \param[out]      executor: Fresh storage with no previous outstanding loans.
 * \param[in]       port: Initialized controller exclusively owned by this
 *                  executor; direct clients and other owners must quiesce.
 * \return          Valid executor port, or a zero port for incomplete methods.
 * \note            Startup/task context. Operations use
 * nx_spi_owner_operation_t endpoints on this bus only; membership is checked
 * before admission. No polling fallback is selected. Controller and executor
 * remain live through drain, control acknowledgement and observer exit.
 * QUARANTINED retains both buffer loans. Hardware stop and IRQ/notification
 * destruction are explicit external steps after owner shutdown, not part of
 * this bind.
 */
nx_owner_executor_port_t
nx_spi_async_owner_executor_port(nx_spi_async_owner_executor_t* executor,
                                 const nx_spi_port_t* port);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_SPI_ASYNC_OWNER_H */
