/**
 * \file            stm32_qspi_model.h
 * \brief           QSPI external MMIO/CPU leaves; official transport unchanged
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32_QSPI_MODEL_H
#define NEXUS_STM32_QSPI_MODEL_H
#include "../cmake/stm32_scs_model.h"
#include "stm32_official_model.h"
#include <stdbool.h>
#undef QUADSPI
#define QUADSPI ((QUADSPI_TypeDef*)(uintptr_t)UINT32_C(0x60000100))
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_qspi_model_read(const volatile uint32_t* reg);
void nx_qspi_model_write(volatile uint32_t* reg, uint32_t value);
uint8_t nx_qspi_model_read8(volatile uint32_t* reg);
void nx_qspi_model_write8(volatile uint32_t* reg, uint8_t value);
void nx_qspi_irq_clear(IRQn_Type irq);
bool nx_qspi_privileged(void);
bool nx_qspi_in_isr(void);
bool nx_qspi_masked(void);
void nx_qspi_dsb(void);
uint32_t HAL_GetTick(void);
#ifdef __cplusplus
}
#endif
#undef READ_REG
#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define READ_REG(reg)         nx_qspi_model_read(&(reg))
#define WRITE_REG(reg, value) nx_qspi_model_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, bits)    WRITE_REG((reg), READ_REG(reg) | (uint32_t)(bits))
#define CLEAR_BIT(reg, bits)  WRITE_REG((reg), READ_REG(reg) & ~(uint32_t)(bits))
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG((reg), (READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(set))
#undef NVIC_ClearPendingIRQ
#define NVIC_ClearPendingIRQ(irq) nx_qspi_irq_clear(irq)
#endif
