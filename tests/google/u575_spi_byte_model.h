/**
 * \file            u575_spi_byte_model.h
 * \brief           U575 SPI hardware leaves with production byte access
 * \author          Nexus Team
 */
#ifndef NX_U575_SPI_BYTE_MODEL_H
#define NX_U575_SPI_BYTE_MODEL_H
#include "u575_devices_model.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_u575_spi_byte_poll(void* port);
void nx_u575_spi_official_byte_write(SPI_TypeDef* registers, uint8_t value);
uint8_t nx_u575_spi_official_byte_read(SPI_TypeDef* registers);
extern SPI_TypeDef g_u575_spi_byte_decoy;
#ifdef __cplusplus
}
#endif
/* Keep word/reset/time observations, but execute the real default data ports.
 */
#undef NX_U575_SPI_READ8
#undef NX_U575_SPI_WRITE8
#undef NX_U575_SPI_POLL
#define NX_U575_SPI_POLL(port) nx_u575_spi_byte_poll(port)
/* A hardcoded SPI1 binding must not substitute for the selected controller. */
#undef SPI1
#define SPI1 (&g_u575_spi_byte_decoy)
#endif
