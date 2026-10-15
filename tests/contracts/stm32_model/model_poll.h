/**
 * \file            model_poll.h
 * \brief           Deterministic STM32 hardware phase model hook
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32_MODEL_POLL_H
#define NX_STM32_MODEL_POLL_H
#include <stdint.h>
uint16_t nx_stm32_model_flash_kib(void);
uint32_t nx_stm32_model_device_id(void);
#define NX_STM32_FLASH_KIB() nx_stm32_model_flash_kib()
#define NX_STM32_DEVICE_ID() nx_stm32_model_device_id()
void nx_stm32_model_poll(void* system);
#define NX_STM32_POLL(system) nx_stm32_model_poll(system)
#endif
