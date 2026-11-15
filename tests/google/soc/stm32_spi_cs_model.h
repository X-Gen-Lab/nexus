/**
 * \file            stm32_spi_cs_model.h
 * \brief           External GPIO latch observation for the SPI providers
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_SPI_CS_MODEL_H
#define NEXUS_STM32_SPI_CS_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Optional selected latch whose set or reset command is refused. */
extern volatile uint32_t* g_nx_spi_cs_fault_odr;
extern uint32_t g_nx_spi_cs_hold_low;
extern uint32_t g_nx_spi_cs_hold_high;
/** \brief Reset fault injection without changing modeled hardware. */
void nx_stm32_spi_cs_model_reset(void);
/** \brief Apply write-only BSRR semantics before reading the output latch. */
uint32_t nx_stm32_spi_cs_model_read(volatile uint32_t* odr,
                                    volatile uint32_t* command);
#ifdef __cplusplus
}
#endif
#define NX_STM32_SPI_CS_READ(gpio)                                             \
    nx_stm32_spi_cs_model_read(&(gpio)->ODR, &(gpio)->BSRR)
#endif
