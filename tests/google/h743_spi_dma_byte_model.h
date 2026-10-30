/**
 * \file            h743_spi_dma_byte_model.h
 * \brief           H743 DMA hardware facts with the real default RX byte port
 * \author          Nexus Team
 */
#ifndef NX_H743_SPI_DMA_BYTE_MODEL_H
#define NX_H743_SPI_DMA_BYTE_MODEL_H
#include "h743_spi_dma_model.h"
/* Keep the complete DMA/cache/IRQ hardware model, but execute the actual
 * production byte read. RAM cannot pop a hardware FIFO on a volatile load;
 * the test explicitly supplies its later empty event and retains the loan
 * until that event. The actual C translation unit supplies width evidence.
 */
#undef NX_H743_SPI_READ8
/* A hardcoded peripheral address must not replace the selected model SPI. */
#undef SPI4
#define SPI4 ((SPI_TypeDef*)0U)
#endif
