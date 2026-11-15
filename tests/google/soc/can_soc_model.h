/**
 * \file            can_soc_model.h
 * \brief           Host-only exact CAN MMIO and NVIC leaf model
 * \author          Nexus Team
 */
#ifndef NEXUS_CAN_SOC_MODEL_H
#define NEXUS_CAN_SOC_MODEL_H
#include "can_access.h"
#ifdef NX_CAN_FACTORY_STM32
#include "stm32f407xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Cold factory checks observe real host NVIC leaf storage. */
extern NVIC_Type g_nx_can_factory_nvic;
#ifdef __cplusplus
}
#endif
#undef NVIC
#define NVIC (&g_nx_can_factory_nvic)
#endif
#define NX_CAN_SOC_IRQ_GROUP() 3U
#endif
