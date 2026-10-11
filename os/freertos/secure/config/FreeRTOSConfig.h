/**
 * \file            FreeRTOSConfig.h
 * \brief           Secure companion CPU capability contract, without a kernel
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_SECURE_COMPANION_CONFIG_H
#define NEXUS_SECURE_COMPANION_CONFIG_H
#include "nexus_config.h"
#define configENABLE_FPU (NEXUS_CPU_HAS_FPU || NEXUS_CPU_HAS_MVE)
#define configENABLE_MVE NEXUS_CPU_HAS_MVE
#define configENABLE_MPU 0
#endif /* NEXUS_SECURE_COMPANION_CONFIG_H */
