/**
 * \file            f407_fsmc_model.h
 * \brief           Host fault observations and settled FSMC data-register port
 * \author          Nexus Team
 */
#ifndef NEXUS_F407_FSMC_MODEL_H
#define NEXUS_F407_FSMC_MODEL_H
#include "stm32_official_model.h"
#include "stm32f4xx_ll_fsmc.h"
/* A0000000h is in the 64-bit ASan shadow gap. Relocate only the modeled
 * register structs into low memory; production keeps the official addresses. */
#undef FSMC_Bank1
#undef FSMC_Bank1E
#define FSMC_Bank1  ((FSMC_Bank1_TypeDef*)(uintptr_t)UINT32_C(0x60000000))
#define FSMC_Bank1E ((FSMC_Bank1E_TypeDef*)(uintptr_t)UINT32_C(0x60000104))
/* Device masks are unsigned long on the SDK's 32-bit target. Model the same
 * 32-bit complement on this 64-bit host without relaxing owned warnings. */
#undef CLEAR_BIT
#define CLEAR_BIT(reg, bits) ((reg) &= ~(uint32_t)(bits))
#undef MODIFY_REG
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG((reg), ((READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(set)))
#undef __FSMC_NORSRAM_DISABLE
#define __FSMC_NORSRAM_DISABLE(device, bank)                                   \
    CLEAR_BIT((device)->BTCR[(bank)], (uint32_t)FSMC_BCR1_MBKEN)
#undef __FSMC_PCCARD_DISABLE
#define __FSMC_PCCARD_DISABLE(device)                                          \
    CLEAR_BIT((device)->PCR4, (uint32_t)FSMC_PCR4_PBKEN)
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_fsmc_model_read(const volatile uint32_t* reg);
uint16_t nx_fsmc_model_read16(const volatile uint16_t* address);
void nx_fsmc_model_write16(volatile uint16_t* address, uint16_t value);
#ifdef __cplusplus
}
#endif
#define NX_STM32_FSMC_READ(reg)       nx_fsmc_model_read(&(reg))
#define NX_STM32_FSMC_READ16(address) nx_fsmc_model_read16(address)
#define NX_STM32_FSMC_WRITE16(address, value)                                  \
    nx_fsmc_model_write16(address, value)
#endif
