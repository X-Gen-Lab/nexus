/**
 * \file            stm32_chip_model.h
 * \brief           CPU-readable identity ports around unchanged official LL
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_CHIP_MODEL_H
#define NEXUS_STM32_CHIP_MODEL_H
#include "stm32f4xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t g_nx_chip_model_uid[3];
extern uint16_t g_nx_chip_model_flash;
extern DBGMCU_TypeDef g_nx_chip_model_debug;
#ifdef __cplusplus
}
#endif
#undef UID_BASE
#undef FLASHSIZE_BASE
#undef DBGMCU
#define UID_BASE       ((uintptr_t)g_nx_chip_model_uid)
#define FLASHSIZE_BASE ((uintptr_t)&g_nx_chip_model_flash)
#define DBGMCU         (&g_nx_chip_model_debug)
#endif
