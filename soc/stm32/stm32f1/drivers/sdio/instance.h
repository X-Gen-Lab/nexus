/**
 * \file            instance.h
 * \brief           Private STM32F103ZE SD memory block binding.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_SDIO_INSTANCE_H
#define NEXUS_STM32F1_SDIO_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "drivers/sdio/transport.h"
#include "nexus/io/block.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One exact socket with owned pad and host receipts. */
typedef struct {
    nx_stm32f1_pads_t port_c;
    nx_stm32f1_pads_t port_d;
    nx_block_geometry_t geometry;
    nx_stm32f1_sdio_t host;
    nx_block_state_t state;
    uint16_t rca;
    bool removed;
    bool allow_write;
#if defined(NX_STM32F1_SDIO_RX_DMA)
    uint32_t dma_sector[128];
#endif
#if defined(NX_STM32F1_SDIO_4BIT)
    bool wide_requested;
    bool wide_receipt;
#endif
} nx_stm32f1_sdio_block_t;

/** \brief Acquire the exact 1-bit socket pads without probing its card. */
nx_result_t nx_stm32f1_sdio_block_prepare(nx_stm32f1_sdio_block_t* state);
/** \brief Withdraw the card and host before restoring socket pads. */
nx_result_t nx_stm32f1_sdio_block_close(nx_stm32f1_sdio_block_t* state);

/** \brief Acquire a socket explicitly allowed to request CMD24 writes. */
nx_result_t nx_stm32f1_sdio_block_prepare_write(nx_stm32f1_sdio_block_t* state);

#if defined(NX_STM32F1_SDIO_4BIT)
/** \brief Acquire all four data pads for a read-only SDHC session. */
nx_result_t
nx_stm32f1_sdio_block_prepare_four_bit(nx_stm32f1_sdio_block_t* state);
/** \brief Acquire the four-lane socket with explicit CMD24 admission. */
nx_result_t
nx_stm32f1_sdio_block_prepare_four_bit_write(nx_stm32f1_sdio_block_t* state);
#endif

#if defined(NX_STM32F1_SDIO_RX_DMA)
nx_result_t
nx_stm32f1_sdio_block_prepare_four_bit_dma(nx_stm32f1_sdio_block_t* state);
#endif

extern const nx_block_ops_t nx_stm32f1_sdio_block_ops;
extern const nx_block_ops_t nx_stm32f1_sdio_block_write_ops;

#ifdef __cplusplus
}
#endif
#endif
