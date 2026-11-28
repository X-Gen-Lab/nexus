/**
 * \file            platform.h
 * \brief           Immutable F1 Platform connection and live time ownership.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_PLATFORM_H
#define NEXUS_STM32F1_PLATFORM_H

#include "nexus/core/platform_lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Flash capacity supplied by the exact assembly's device facts. */
typedef struct {
    uint32_t flash_bytes;
} nx_stm32f1_platform_config_t;

/** \brief Cold Thread acquisition of one reset HSI8 system and TIM2. */
nx_platform_start_result_t
nx_stm32f1_platform_system_start(const nx_stm32f1_platform_config_t* config);

/** \brief Validate the fixed connection when no shared resources are selected.
 */
nx_result_t
nx_stm32f1_platform_no_resources(const nx_stm32f1_platform_config_t* config);

/** \brief Cold Thread withdrawal, retriable until all effects are released. */
nx_result_t
nx_stm32f1_platform_system_release(const nx_stm32f1_platform_config_t* config,
                                   bool started);

/** \brief Observe the selected Core deadline epoch in privileged context. */
bool nx_stm32f1_system_live(void);

/** \brief Dedicated update IRQ, independent from the selected OS tick. */
void TIM2_IRQHandler(void);

#ifdef __cplusplus
}
#endif
#endif
