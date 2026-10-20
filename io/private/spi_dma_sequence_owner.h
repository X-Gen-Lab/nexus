/**
 * \file            spi_dma_sequence_owner.h
 * \brief           Optional owner identity for exact segmented sequence faces
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_SPI_DMA_SEQUENCE_OWNER_H
#define NEXUS_PRIVATE_SPI_DMA_SEQUENCE_OWNER_H
#include "nexus/io/spi_dma_owner_binding.h"
#include "spi_dma_sequence.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact shared method tables and controller identity, no hardware. */
bool nx_spi_dma_sequence_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
