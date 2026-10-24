/** \file stm32u575_lpdma_copy_hw.h
 * \brief Exact U575 copy MMIO boundary; vendor types remain private.
 * \author Nexus Team
 */
#ifndef NX_STM32U575_LPDMA_COPY_HW_H
#define NX_STM32U575_LPDMA_COPY_HW_H
#include "stm32u575xx.h"
#ifdef NX_U575_LPDMA_COPY_MODEL
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_u575_lpdma_copy_read(uintptr_t address);
void nx_u575_lpdma_copy_write(uintptr_t address, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_LPDMA_READ(reg) nx_u575_lpdma_copy_read((uintptr_t) & (reg))
#define NX_LPDMA_WRITE(reg, value)                                             \
    nx_u575_lpdma_copy_write((uintptr_t) & (reg), (uint32_t)(value))
#else
#if !defined(__ARM_ARCH_8M_MAIN__)
#error "U575 LPDMA requires the selected Cortex-M33 compiler profile"
#endif
#define NX_LPDMA_READ(reg)         ((uint32_t)(reg))
#define NX_LPDMA_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#endif
