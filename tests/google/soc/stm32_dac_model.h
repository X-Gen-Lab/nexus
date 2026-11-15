/**
 * \file            stm32_dac_model.h
 * \brief           Low-address MMIO events around unchanged official LL bodies
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_DAC_MODEL_H
#define NEXUS_STM32_DAC_MODEL_H
#include "stm32_official_model.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_dac_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define WRITE_REG(reg, value)              nx_dac_model_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, bits)                 WRITE_REG((reg), (reg) | (bits))
#define CLEAR_BIT(reg, bits)               WRITE_REG((reg), (reg) & ~(bits))
#define MODIFY_REG(reg, clear, set)        WRITE_REG((reg), ((reg) & ~(clear)) | (set))
#define NX_EXTI_HARDWARE_WRITE(reg, value) nx_dac_model_write((reg), (value))
#define NX_EXTI_HARDWARE_CLEAR(reg, mask)  nx_dac_model_write((reg), (mask))
#endif
