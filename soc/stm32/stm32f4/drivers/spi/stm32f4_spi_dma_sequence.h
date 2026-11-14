/**
 * \file            stm32f4_spi_dma_sequence.h
 *
 * \brief           Selected F4 SPI sequence backend declaration
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_SPI_DMA_SEQUENCE_H
#define NEXUS_STM32F4_SPI_DMA_SEQUENCE_H

#include "spi_dma_sequence.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Fixed SPI sequence transport; one immutable backend per provider. */
extern const nx_spi_dma_sequence_backend_t nx_stm32_spi_dma_sequence_backend;

#ifdef __cplusplus
}
#endif

#endif
