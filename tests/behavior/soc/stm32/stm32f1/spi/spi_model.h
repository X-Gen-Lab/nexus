/**
 * \file            spi_model.h
 * \brief           F103 SPI2 and GPIO latch external register model.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_SPI_MODEL_H
#define NEXUS_TEST_F1_SPI_MODEL_H
#include "stm32f1xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_spi_model_write(volatile uint32_t* reg, uint32_t value);
void nx_spi_model_bsrr(GPIO_TypeDef* reg, uint32_t value);
uint32_t nx_spi_model_status(SPI_TypeDef* reg);
void nx_spi_model_send(SPI_TypeDef* reg, uint16_t value, uint8_t bits);
uint16_t nx_spi_model_receive(SPI_TypeDef* reg, uint8_t bits);
#ifdef __cplusplus
}
#endif
#define NX_STM32F1_WRITE(reg, value)     nx_spi_model_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value) nx_spi_model_bsrr((reg), (value))
#define NX_STM32F1_SPI_CS(reg, mask, active)                                   \
    nx_spi_model_bsrr((reg), (active) ? ((mask) << 16U) : (mask))
#define NX_STM32F1_SPI_STATUS(reg) nx_spi_model_status(reg)
#define NX_STM32F1_SPI_SEND(reg, value, bits)                                  \
    nx_spi_model_send((reg), (value), (bits))
#define NX_STM32F1_SPI_RECEIVE(reg, bits) nx_spi_model_receive((reg), (bits))
#endif
