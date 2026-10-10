/**
 * \file            gd32_spi_dma_model.h
 * \brief           Host-only DMA disable and write-one-clear hardware ports
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_SPI_DMA_MODEL_H
#define NEXUS_GD32_SPI_DMA_MODEL_H
#include <stdbool.h>
#include <stdint.h>
/** \brief Inject an engine that refuses enable-bit withdrawal. */
void nx_gd32_spi_dma_model_disable(bool receive, uint32_t mask);
/** \brief Model exact W1C effects without clearing neighbouring channels. */
void nx_gd32_spi_dma_model_ack(bool receive, uint32_t mask);
#define NX_GD32_SPI_DMA_DISABLE(receive, mask)                                 \
    nx_gd32_spi_dma_model_disable((receive), (uint32_t)(mask))
#define NX_GD32_SPI_DMA_ACK(receive, mask)                                     \
    nx_gd32_spi_dma_model_ack((receive), (uint32_t)(mask))
#endif
