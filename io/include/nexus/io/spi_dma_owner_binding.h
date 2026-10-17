/**
 * \file            spi_dma_owner_binding.h
 * \brief           Optional immutable segmented SPI ownership and IRQ binding
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_SPI_DMA_OWNER_BINDING_H
#define NEXUS_IO_SPI_DMA_OWNER_BINDING_H
#include "nexus/io/spi_dma_transaction.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Selected-only provider capability without changing IO faces.
 * \note            Stable immutable storage outlives executor and publishers.
 *                  contains proves actual controller identity; a display name
 *                  or a caller-assembled pair of pointers is not such proof.
 *                  attach_wake validates actual IRQ sources before borrowing
 *                  the sink. A missing hook permits explicit periodic service.
 */
typedef struct {
    const nx_spi_dma_transaction_port_t* port;
    bool (*contains)(const nx_spi_dma_transaction_endpoint_t*,
                     const nx_spi_dma_transaction_port_t*);
    nx_result_t (*attach_wake)(const nx_spi_dma_transaction_port_t*,
                               const nx_irq_wake_t*, uint8_t);
} nx_spi_dma_owner_binding_t;
/**
 * \brief           Check exact endpoint membership without hardware effects.
 * \param[in]       binding: Stable provider-owned immutable capability.
 * \param[in]       endpoint: Stable enhanced endpoint on the selected bus.
 * \return          False for malformed or foreign identities.
 * \note            No acquisition, initialization, allocation or service.
 */
bool nx_spi_dma_owner_contains(
    const nx_spi_dma_owner_binding_t* binding,
    const nx_spi_dma_transaction_endpoint_t* endpoint);
/**
 * \brief           Attach a borrowed hint to actual validated IRQ publishers.
 * \param[in]       binding: Stable selected controller capability.
 * \param[in]       wake: Immutable sink kept live through publisher join, NULL
 *                  to detach under the provider's lifecycle contract.
 * \param[in]       syscall_ceiling: Actual unshifted kernel threshold.
 * \return          SUCCESS, INVALID, CONTEXT, UNSUPPORTED or provider failure.
 * \note            Task-only. Rejection leaves the previous binding intact.
 *                  A hint is never completion. Detach alone does not join a
 *                  publisher that has already captured the sink. Stop and join
 *                  publishers and waiters before destroying notification data.
 */
nx_result_t
nx_spi_dma_owner_attach_wake(const nx_spi_dma_owner_binding_t* binding,
                             const nx_irq_wake_t* wake,
                             uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
