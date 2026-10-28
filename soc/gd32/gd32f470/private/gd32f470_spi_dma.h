/**
 * \file            gd32f470_spi_dma.h
 * \brief           Private SPI4/SPI0 finite full-duplex DMA storage
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_SPI_DMA_H
#define NEXUS_GD32F470_SPI_DMA_H

#include "gd32f470_spi_cold.h"
#include "nexus/io/dma.h"

typedef struct nx_gd32_spi_dma_endpoint_state nx_gd32_spi_dma_endpoint_state_t;

/**
 * \brief           SPI4 DMA1 RX3/TX4 selector2 or SPI0 RX2/TX5 selector3.
 * \note            Board owns reviewed AF/CS and shared DMA1 clock. Immutable
 *                  regions exclude CPU-only TCM. Request, endpoint and buffers
 *                  remain borrowed until both engines and the wire drain. Fresh
 * errors remain latched through completion W1C and final withdrawal.
 */
typedef struct {
    nx_gd32_spi_state_t spi;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_spi_request_t* active;
    nx_gd32_spi_dma_endpoint_state_t* endpoint;
    const nx_irq_wake_t* wake;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    bool tx_complete;
    bool rx_complete;
    bool draining;
    bool stopping;
    bool initialized;
    bool faulted;
    uint8_t memory_flags;
} nx_gd32_spi_dma_state_t;

/** \brief Independent immutable CS/mode/rate binding on one DMA controller. */
struct nx_gd32_spi_dma_endpoint_state {
    nx_gd32_spi_endpoint_state_t endpoint;
    nx_gd32_spi_dma_state_t* dma;
};

/** \brief Immutable selected DMA domains and publisher priority. */
typedef struct {
    nx_gd32_spi_config_t spi;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    uint8_t priority;
    bool kernel_bound;
} nx_gd32_spi_dma_config_t;

/** \brief Pins and IRQ priority leases remain until actual close succeeds. */
typedef struct {
    nx_gd32_spi_cold_t spi;
    uint8_t priorities[2];
    bool vectors;
} nx_gd32_spi_dma_cold_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Prepare exact SPI0/SPI4 pads, endpoints and DMA publishers.
 * \param[in,out] state: Cold serialized owner with no outstanding request.
 * \param[in] config: Immutable exact resources, retained through close.
 * \param[in,out] cold: Zero initial receipt; failed admission retains effects.
 * \return Success or error requiring close to consume any actual acquisitions.
 * \note Privileged task, interrupts unmasked; no allocation or discovery.
 */
nx_result_t nx_gd32_spi_dma_prepare(nx_gd32_spi_dma_state_t* state,
                                    const nx_gd32_spi_dma_config_t* config,
                                    nx_gd32_spi_dma_cold_t* cold);
/** \brief Drain provider, withdraw vectors and restore exact cold resources.
 * \param[in,out] state: Same owner after external callers have been joined.
 * \param[in] config: Unchanged selected resource description.
 * \param[in,out] cold: Actual receipt retained until every effect is restored.
 * \return Success or BUSY/IO retaining responsibility for explicit retry.
 */
nx_result_t nx_gd32_spi_dma_close(nx_gd32_spi_dma_state_t* state,
                                  const nx_gd32_spi_dma_config_t* config,
                                  nx_gd32_spi_dma_cold_t* cold);
extern const nx_spi_ops_t nx_gd32_spi_dma_ops;
extern const nx_spi_endpoint_ops_t nx_gd32_spi_dma_endpoint_ops;
/**
 * \brief           Acquire the exact controller and its two finite DMA vectors.
 * \param[in,out]   state: Cold static storage with immutable memory regions.
 * \param[in,out]   endpoint: Cold first child, kept through successful stop.
 * \param[in]       cs_gpio: Reviewed output GPIO register base.
 * \param[in]       cs_mask: Reviewed nonzero 16-bit CS output mask.
 * \param[in]       maximum_hz: Positive maximum 8-bit MSB-first wire rate.
 * \param[in]       mode: SPI mode zero through three.
 * \param[in]       priority: Unshifted four-bit priority for both DMA vectors.
 * \return          Success or INVALID/CONTEXT/BUSY before source acquisition.
 * \note            Task startup after AF/CS and DMA1 clock preparation. Other
 *                  cold children use nx_gd32_spi_endpoint_initialize and bind
 *                  their dma pointer explicitly. No request is borrowed here.
 */
nx_result_t nx_gd32_spi_dma_initialize(
    nx_gd32_spi_dma_state_t* state, nx_gd32_spi_dma_endpoint_state_t* endpoint,
    uint32_t cs_gpio, uint32_t cs_mask, uint32_t maximum_hz, unsigned mode,
    unsigned priority);
/**
 * \brief           Select SPI4 or SPI0 with their exact finite DMA routes.
 * \param[in,out]   state: Fresh caller-owned state with immutable DMA domains.
 * \param[in,out]   endpoint: First cold addressed child, retained until stop.
 * \param[in]       controller: Exact maintained readonly SPI controller.
 * \param[in]       cs_gpio: Reviewed chip-select output GPIO register base.
 * \param[in]       cs_mask: Reviewed positive 16-bit chip-select mask.
 * \param[in]       maximum_hz: Positive maximum divided APB2 wire frequency.
 * \param[in]       mode: SPI mode zero through three.
 * \param[in]       priority: Unshifted priority for both DMA publishers.
 * \return          Success or INVALID/CONTEXT/BUSY before request admission.
 * \note            SPI4 uses DMA1 channel3 RX/channel4 TX selector2; SPI0 uses
 *                  DMA1 channel2 RX/channel5 TX selector3. Assembly owns shared
 *                  DMA1 clock and reviewed pins/CS. No hidden scalar buffers.
 */
nx_result_t nx_gd32_spi_dma_initialize_at(
    nx_gd32_spi_dma_state_t* state, nx_gd32_spi_dma_endpoint_state_t* endpoint,
    const nx_gd32_spi_controller_t* controller, uint32_t cs_gpio,
    uint32_t cs_mask, uint32_t maximum_hz, unsigned mode, unsigned priority);
/**
 * \brief           Latch one exact DMA terminal source without settling in IRQ.
 * \param[in,out]   state: Same static state retained by both generated vectors.
 * \param[in]       receive: True for DMA1 channel3, false for channel4.
 * \note            Task service establishes independent wire-idle proof.
 */
void nx_gd32_spi_dma_irq(nx_gd32_spi_dma_state_t* state, bool receive);
#ifdef __cplusplus
}
#endif
#endif
