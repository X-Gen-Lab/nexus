/**
 * \file            stm32_memory_boot_model.h
 * \brief           Explicit reset-memory barrier and terminal fault hooks
 * \author          Nexus Team
 */
#ifndef NX_STM32_MEMORY_BOOT_MODEL_H
#define NX_STM32_MEMORY_BOOT_MODEL_H
#include "nexus/arch/arch.h"
#include "stm32f407xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe/refuse the CCM clock at the real reset barrier boundary. */
void nx_stm32_memory_model_barrier(void);
/** \brief Terminal child-process witness; never returns into reset startup. */
void nx_stm32_memory_model_failure(unsigned code);
#ifdef __cplusplus
}
#endif
#define nx_arch_dsb()               nx_stm32_memory_model_barrier()
#define NX_STM32_BOOT_FAILURE(code) nx_stm32_memory_model_failure(code)
#endif
