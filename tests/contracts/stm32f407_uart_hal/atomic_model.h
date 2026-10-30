/**
 * \file            atomic_model.h
 * \brief           Host compiler bridge followed by the selected atomic port
 * \author          Nexus Team
 */
#ifndef NX_F407_ATOMIC_CONTRACT_MODEL_H
#define NX_F407_ATOMIC_CONTRACT_MODEL_H
#include "stm32_official_model.h"
#include "stm32f407_hal_atomic.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_atomic_vendor_enable_tx(USART_TypeDef* uart);
void nx_atomic_vendor_disable_tx(USART_TypeDef* uart);
#ifdef __cplusplus
}
#endif
#endif
