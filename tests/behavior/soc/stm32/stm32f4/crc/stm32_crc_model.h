/**
 * \file            stm32_crc_model.h
 * \brief           External CRC register writes around the official LL bodies
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_CRC_MODEL_H
#define NEXUS_STM32_CRC_MODEL_H
#include "stm32f4xx.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_crc_model_write(volatile uint32_t* address, uint32_t value);
#ifdef __cplusplus
}
#endif
#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define WRITE_REG(reg, value)       nx_crc_model_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, bits)          WRITE_REG((reg), (reg) | (bits))
#define CLEAR_BIT(reg, bits)        WRITE_REG((reg), (reg) & ~(bits))
#define MODIFY_REG(reg, clear, set) WRITE_REG((reg), ((reg) & ~(clear)) | (set))
#endif
