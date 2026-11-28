/**
 * \file            spi_dma_sequence_backend.h
 * \brief           Optional STM32F103 SPI2 finite DMA sequence backend.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_SPI_DMA_SEQUENCE_BACKEND_H
#define NEXUS_STM32F1_SPI_DMA_SEQUENCE_BACKEND_H

#include "drivers/spi/spi_dma.h"
#include "spi_dma_sequence_owner.h"

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_spi_dma_sequence_backend_t nx_stm32f1_spi_dma_sequence_backend;

/** \brief Check the generated sequence's exact physical child identity. */
bool nx_stm32f1_spi_dma_owner_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port);

/** \brief This finite provider completes via task-side service. */
nx_result_t
nx_stm32f1_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t syscall_ceiling);

/** \brief Retire the sequence before restoring the physical owner. */
nx_result_t
nx_stm32f1_spi_dma_sequence_close(nx_spi_dma_sequence_state_t* sequence,
                                  nx_stm32f1_spi_dma_t* owner);
#ifdef __cplusplus
}
#endif
#endif
