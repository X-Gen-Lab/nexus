/**
 * \file            platform_coordinator.c
 * \brief           F4 system startup and fixed shared-owner withdrawal order.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_platform_lifecycle_impl.h"

nx_platform_start_result_t
nx_stm32f4_platform_system_start(const nx_stm32f4_platform_config_t* config) {
    return nx_stm32f4_platform_system_start_local(config);
}

nx_result_t nx_stm32f4_platform_resources_acquire(
    const nx_stm32f4_platform_config_t* config) {
    return nx_stm32f4_platform_resources_acquire_local(config);
}

nx_result_t nx_stm32f4_platform_resources_release(
    const nx_stm32f4_platform_config_t* config) {
    return nx_stm32f4_platform_resources_release_local(config);
}

nx_result_t
nx_stm32f4_platform_system_release(const nx_stm32f4_platform_config_t* config,
                                   bool started) {
    return nx_stm32f4_platform_system_release_local(config, started);
}
