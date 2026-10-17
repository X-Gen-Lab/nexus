/**
 * \file            spi_dma_sequence_model.h
 * \brief           Observe final asynchronous DMA engine withdrawal readback
 * \author          Nexus Team
 */
#ifndef NEXUS_SPI_DMA_SEQUENCE_MODEL_H
#define NEXUS_SPI_DMA_SEQUENCE_MODEL_H
#include "spi_multi_controller_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model the last bus-master beat before disabled EN is observed. */
uint32_t nx_spi_dma_sequence_model_read(volatile uint32_t* control);
#ifdef __cplusplus
}
#endif
#define NX_SPI_DMA_SEQUENCE_ENGINE_READ(control)                               \
    nx_spi_dma_sequence_model_read((control))
#endif
