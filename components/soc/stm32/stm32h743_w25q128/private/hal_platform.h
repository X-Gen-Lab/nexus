/**
 * \file            hal_platform.h
 * \brief           Private official HAL clock view over actual Nexus SoC facts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_H743_W25Q128_HAL_PLATFORM_H
#define NX_H743_W25Q128_HAL_PLATFORM_H
/* Consume original CMSIS declarations before the private expression mapping. */
#include "stm32h743_system.h"
#include "stm32h7xx_hal.h"
/* The selected System owns and updates core_hz. No duplicate variable, guessed
 * peripheral frequency or general CMSIS clock API is introduced. */
#define SystemCoreClock (g_nx_stm32h743_system.core_hz)
#endif
