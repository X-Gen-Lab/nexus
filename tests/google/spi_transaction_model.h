/**
 * \file            spi_transaction_model.h
 * \brief           Whole-CS observations and SPI data-register model effects
 * \author          Nexus Team
 */
#ifndef NEXUS_SPI_TRANSACTION_MODEL_H
#define NEXUS_SPI_TRANSACTION_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model the selected hardware data-register write and received word. */
void nx_spi_transaction_model_write(void* port, uint16_t word, uint8_t bits);
/** \brief Observe CS only after the production GPIO operation succeeds. */
void nx_spi_transaction_model_cs(bool active);
#ifdef __cplusplus
}
#endif
#define NX_STM32_SPI_TRANSACTION_WRITE(port, word, bits)                       \
    nx_spi_transaction_model_write((port), (word), (bits))
#define NX_GD32_SPI_TRANSACTION_WRITE(port, word, bits)                        \
    nx_spi_transaction_model_write((port), (word), (bits))
#define NX_STM32_SPI_TRANSACTION_CS(active)                                    \
    nx_spi_transaction_model_cs((active))
#define NX_GD32_SPI_TRANSACTION_CS(active) nx_spi_transaction_model_cs((active))
#endif
