/**
 * \file            gd32f470_spi_dma.h
 * \brief           Private fixed SPI4 finite full-duplex DMA storage
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_SPI_DMA_H
#define NEXUS_GD32F470_SPI_DMA_H

#include "gd32f470_provider.h"
#include "nexus/io/dma.h"

typedef struct nx_gd32_spi_dma_endpoint_state nx_gd32_spi_dma_endpoint_state_t;

/**
 * \brief           Exact SPI4 DMA1 channel3 RX / channel4 TX selector2 state.
 * \note            Board owns reviewed AF/CS and shared DMA1 clock. Immutable
 *                  regions exclude CPU-only TCM. Request, endpoint and buffers
 *                  remain borrowed until both engines and the wire drain.
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
} nx_gd32_spi_dma_state_t;

/** \brief Independent immutable CS/mode/rate binding on one DMA controller. */
struct nx_gd32_spi_dma_endpoint_state {
    nx_gd32_spi_endpoint_state_t endpoint;
    nx_gd32_spi_dma_state_t* dma;
};

#ifdef __cplusplus
extern "C" {
#endif
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
