/**
 * \file            spi_multi_controller_model.h
 * \brief           Exact DMA W1C/disable and peripheral reset boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_SPI_MULTI_CONTROLLER_MODEL_H
#define NEXUS_SPI_MULTI_CONTROLLER_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply W1C to exactly the selected DMA interrupt bank. */
void nx_spi_multi_dma_ack(volatile uint32_t* clear, uint32_t mask);
/** \brief Fault only the selected EN readback, preserving all other engines. */
void nx_spi_multi_dma_disable(volatile uint32_t* control, uint32_t mask);
/** \brief Model the vendor's selected reset register and reset pulse. */
void nx_spi_multi_reset(volatile uint32_t* reset, uint32_t mask, bool active);
#ifdef __cplusplus
}
#endif
#define NX_STM32_DMA_DISABLE(stream, mask)                                     \
    nx_spi_multi_dma_disable(&(stream)->CR, (mask))
#define NX_STM32_SPI_DMA_ACK(clear, mask) nx_spi_multi_dma_ack((clear), (mask))
#define NX_GD32_SPI_DMA_CONTROL_DISABLE(control, mask)                         \
    nx_spi_multi_dma_disable((control), (mask))
#define NX_GD32_SPI_DMA_FLAGS_ACK(clear, mask)                                 \
    nx_spi_multi_dma_ack((clear), (mask))
#define NX_STM32_SPI_RESET(reset, mask, active)                                \
    nx_spi_multi_reset((reset), (mask), (active))
#endif
