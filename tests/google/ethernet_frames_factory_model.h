/**
 * \file            ethernet_frames_factory_model.h
 * \brief           Shared real System, GPIO and enhanced MAC external oracle
 * \author          Nexus Team
 */
#ifndef NEXUS_ETH_FRAMES_FACTORY_MODEL_H
#define NEXUS_ETH_FRAMES_FACTORY_MODEL_H
#include "ethernet_frames_model.h"
#include <stdint.h>
#ifdef NX_ETH_FRAMES_FACTORY_STM32
#include "stm32_system_model.h"
#include "stm32f407xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern NVIC_Type g_nx_eth_frames_factory_nvic;
uint16_t nx_eth_frames_factory_flash_kib(void);
uint32_t nx_eth_frames_factory_device_id(void);
#ifdef __cplusplus
}
#endif
#undef NVIC
#define NVIC                 (&g_nx_eth_frames_factory_nvic)
#define NX_STM32_FLASH_KIB() nx_eth_frames_factory_flash_kib()
#define NX_STM32_DEVICE_ID() nx_eth_frames_factory_device_id()
#else
#include "gd32_system_model.h"
#include "gd32_timebase_model.h"
#include "gd32f4xx.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
void nx_eth_frames_factory_register_write(volatile uint32_t* reg,
                                          uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_eth_frames_factory_register_write(&(reg), (uint32_t)(value))
#endif
