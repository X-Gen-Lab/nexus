/**
 * \file            fault_model.h
 * \brief           Inject exact Cortex-M3 fault register and storage state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F103_FAULT_MODEL_H
#define NEXUS_STM32F103_FAULT_MODEL_H
#include "nexus/components/fault_record.h"
#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern SCB_Type f103_fault_scb;
extern nx_fault_record_t f103_fault_storage;
bool f103_fault_privileged(void);
#ifdef __cplusplus
}
#endif

#define NX_F1_FAULT_SCB        (&f103_fault_scb)
#define NX_F1_FAULT_SRAM_START ((uintptr_t)&f103_fault_storage)
#define NX_F1_FAULT_SRAM_END                                                   \
    (NX_F1_FAULT_SRAM_START + sizeof(f103_fault_storage))
#define nx_arch_is_privileged f103_fault_privileged
#endif /* NEXUS_STM32F103_FAULT_MODEL_H */
