/**
 * \file            ethernet_factory_model.h
 * \brief           External cold owned-register write boundary for assembly
 * \author          Nexus Team
 */
#ifndef NEXUS_ETHERNET_FACTORY_MODEL_H
#define NEXUS_ETHERNET_FACTORY_MODEL_H
#include "stm32f407xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Selected cold checks use actual host NVIC leaf storage. */
extern NVIC_Type g_nx_eth_factory_nvic;
typedef __UINT32_TYPE__ nx_eth_factory_register_t;
void nx_eth_factory_register_write(volatile nx_eth_factory_register_t* address,
                                   nx_eth_factory_register_t value);
#ifdef __cplusplus
}
#endif
#undef NVIC
#define NVIC (&g_nx_eth_factory_nvic)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_eth_factory_register_write(&(reg), (nx_eth_factory_register_t)(value))
#endif
