/**
 * \file            stm32_i2c_storage_model.c
 * \brief           External system storage for isolated I2C register fixtures
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

nx_stm32_system_t g_nx_stm32_system;
#ifdef NEXUS_STM32_MODEL
GPIO_TypeDef g_nx_stm32_gpioa_model;
#endif
