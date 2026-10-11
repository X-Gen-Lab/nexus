/**
 * \file            FreeRTOSConfig.h
 * \brief           Pinned kernel declarations for real tickless mechanism tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_LOWPOWER_TEST_CONFIG_H
#define NEXUS_LOWPOWER_TEST_CONFIG_H
#include "../../contracts/os_freertos_runtime/FreeRTOSConfig.h"
#define configUSE_TICKLESS_IDLE 2
#endif /* NEXUS_LOWPOWER_TEST_CONFIG_H */
