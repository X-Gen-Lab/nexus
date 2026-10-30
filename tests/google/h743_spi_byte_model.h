/**
 * \file            h743_spi_byte_model.h
 * \brief           H743 hardware observations with real default byte ports
 * \author          Nexus Team
 */
#ifndef NX_H743_SPI_BYTE_MODEL_H
#define NX_H743_SPI_BYTE_MODEL_H
#include "h743_peripheral_model.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_h743_spi_byte_poll(void* port);
void nx_h743_spi_official_byte_write(SPI_TypeDef* registers, uint8_t value);
uint8_t nx_h743_spi_official_byte_read(SPI_TypeDef* registers);
extern SPI_TypeDef g_h743_spi_byte_decoy;
#ifdef __cplusplus
}
#endif
/* Preserve external clock/reset/word observations, but execute the production
 * byte load/store. Polling supplies external FIFO and wire-ready facts only.
 */
#undef NX_H743_SPI_READ8
#undef NX_H743_SPI_WRITE8
#undef NX_H743_SPI_POLL
#define NX_H743_SPI_POLL(port) nx_h743_spi_byte_poll(port)
#undef SPI4
#define SPI4 (&g_h743_spi_byte_decoy)
#endif
