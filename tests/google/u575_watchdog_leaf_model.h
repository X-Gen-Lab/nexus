/**
 * \file            u575_watchdog_leaf_model.h
 * \brief           Real IWDG defaults with external LSI facts
 * \author          Nexus Team
 */
#ifndef NX_U575_WATCHDOG_LEAF_MODEL_H
#define NX_U575_WATCHDOG_LEAF_MODEL_H
#define NX_U575_WATCHDOG_DEFAULT_PORT 1
#include "u575_model.h"

#include "stm32u575_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_u575_iwdg_official_commands(IWDG_TypeDef* registers, uint32_t prescaler,
                                    uint32_t reload, uint32_t keys[4]);
#ifdef __cplusplus
}
#endif
#endif
