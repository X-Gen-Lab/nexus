/**
 * \file            stm32_sdram_model.h
 * \brief           F479 register effects at external MMIO and CPU boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32_SDRAM_MODEL_H
#define NEXUS_STM32_SDRAM_MODEL_H
#include "stm32_official_model.h"
#include "stm32f4xx_ll_fmc.h"
#include <stdbool.h>
#undef FMC_Bank5_6
#define FMC_Bank5_6 ((FMC_Bank5_6_TypeDef*)(uintptr_t)UINT32_C(0x60000140))
#undef FMC_Bank1
#define FMC_Bank1 ((FMC_Bank1_TypeDef*)(uintptr_t)UINT32_C(0x60000000))
#undef FMC_SDRAM_DEVICE
#define FMC_SDRAM_DEVICE FMC_Bank5_6
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_sdram_model_read(const volatile uint32_t* reg);
void nx_sdram_model_write(volatile uint32_t* reg, uint32_t value);
bool nx_sdram_privileged(void);
bool nx_sdram_in_isr(void);
bool nx_sdram_masked(void);
void nx_sdram_dsb(void);
uint32_t HAL_GetTick(void);
#ifdef __cplusplus
}
#endif
#undef READ_REG
#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define READ_REG(reg)         nx_sdram_model_read(&(reg))
#define WRITE_REG(reg, value) nx_sdram_model_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, bits)    WRITE_REG((reg), READ_REG(reg) | (uint32_t)(bits))
#define CLEAR_BIT(reg, bits)  WRITE_REG((reg), READ_REG(reg) & ~(uint32_t)(bits))
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG((reg), (READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(set))
/* Model the SDK target's 32-bit complement without relaxing warnings. */
#undef __FMC_NORSRAM_DISABLE
#define __FMC_NORSRAM_DISABLE(device, bank)                                    \
    CLEAR_BIT((device)->BTCR[(bank)], FMC_BCR1_MBKEN)
#endif
