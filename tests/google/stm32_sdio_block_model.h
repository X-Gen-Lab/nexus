/**
 * \file            stm32_sdio_block_model.h
 * \brief           External SDIO command, FIFO and register observations
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_SDIO_BLOCK_MODEL_H
#define NEXUS_STM32_SDIO_BLOCK_MODEL_H
#include "stm32_official_model.h"
#include "stm32_platform_cold_model.h"
#include "stm32f4xx_ll_sdmmc.h"

#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_sdio_model_read(volatile const uint32_t* reg);
void nx_sdio_model_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_sdio_model_fifo_read(SDIO_TypeDef* registers);
HAL_StatusTypeDef nx_sdio_model_fifo_write(SDIO_TypeDef* registers,
                                           uint32_t* word);
#ifdef __cplusplus
}
#endif

#define NX_SDIO_READ(reg)            nx_sdio_model_read(&(reg))
#define NX_SDIO_WRITE(reg, value)    nx_sdio_model_write(&(reg), (uint32_t)(value))
#define NX_SDIO_FIFO_READ(registers) nx_sdio_model_fifo_read(registers)
#define NX_SDIO_FIFO_WRITE(registers, word)                                    \
    nx_sdio_model_fifo_write((registers), (word))
#undef READ_REG
#undef WRITE_REG
#undef MODIFY_REG
#undef SET_BIT
#undef CLEAR_BIT
#define READ_REG(reg)         nx_sdio_model_read(&(reg))
#define WRITE_REG(reg, value) nx_sdio_model_write(&(reg), (uint32_t)(value))
#define MODIFY_REG(reg, clear, value)                                          \
    WRITE_REG(reg, (READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(value))
#define SET_BIT(reg, value)   WRITE_REG(reg, READ_REG(reg) | (uint32_t)(value))
#define CLEAR_BIT(reg, value) WRITE_REG(reg, READ_REG(reg) & ~(uint32_t)(value))
#endif
