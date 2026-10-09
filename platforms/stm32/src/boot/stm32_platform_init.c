/**
 * \file            stm32_platform_init.c
 * \brief           STM32 platform initialization implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-28
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements platform initialization for STM32 series.
 *                  Integrates HAL_Init() and clock configuration, configures
 *                  NVIC priority grouping, and provides platform initialization
 *                  interface for Nexus framework.
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of Nexus framework.
 *
 * Author:          Nexus Team
 */

#include "nexus_config.h"
#include "boot/stm32_boot.h"
#include "hal/nx_status.h"
#include "hal/provider/nx_device_provider.h"
#include "arch/nx_arch.h"
#include "osal/osal.h"
#ifdef NX_CONFIG_OSAL_BAREMETAL
#include "osal/osal_baremetal.h"
#endif
#include "clock/stm32_clock.h"
#include "system/stm32_performance.h"

#if defined(STM32F407xx) || defined(STM32F429xx) || defined(STM32F446xx) ||    \
    defined(STM32F4)
#include "stm32f4xx_hal.h"
#elif defined(STM32H743xx) || defined(STM32H750xx) || defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32L476xx) || defined(STM32L432xx) || defined(STM32L4)
#include "stm32l4xx_hal.h"
#else
#error "Unsupported STM32 series"
#endif

/*---------------------------------------------------------------------------*/
/* Private definitions                                                       */
/*---------------------------------------------------------------------------*/

/* Default NVIC priority grouping */
#ifndef NX_CONFIG_STM32_NVIC_PRIORITY_GROUP
#define NX_CONFIG_STM32_NVIC_PRIORITY_GROUP 4
#endif
#if NX_CONFIG_STM32_NVIC_PRIORITY_GROUP < 0 || NX_CONFIG_STM32_NVIC_PRIORITY_GROUP > 4
#error "STM32 NVIC priority group must be a logical value from 0 to 4"
#endif
#if defined(NX_CONFIG_OSAL_FREERTOS) && NX_CONFIG_STM32_NVIC_PRIORITY_GROUP != 4
#error "FreeRTOS requires all STM32 priority bits assigned to preemption"
#endif

/* Default SysTick priority */
#ifndef NX_CONFIG_STM32_SYSTICK_PRIORITY
#define NX_CONFIG_STM32_SYSTICK_PRIORITY 15
#endif

/* Default PendSV priority */
#ifndef NX_CONFIG_STM32_PENDSV_PRIORITY
#define NX_CONFIG_STM32_PENDSV_PRIORITY 15
#endif

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

static bool platform_owned;
static bool platform_ready;
#ifdef NX_CONFIG_OSAL_BAREMETAL
static bool clock_bound;
#endif

/* Discovery has no external safety policy; reviewed boards override the narrow
 * safe-initial-level hook. GPIO banks may become high impedance during vendor
 * reset. Continuous actuator safety is a product/electrical responsibility. */
__weak nx_status_t nx_board_prepare_safe_outputs(void) { return NX_OK; }
extern nx_status_t nx_stm32f407_resources_idle(void);

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize STM32 platform
 * \details         Performs complete platform initialization including:
 *                  - HAL library initialization (includes SysTick 1ms setup)
 *                  - System clock configuration
 *                  - NVIC priority grouping configuration
 *                  - SysTick and PendSV priority configuration
 *                  - Performance measurement initialization
 * \note            This function should be called early in main() before
 *                  any peripheral initialization.
 *                  SysTick timer is automatically configured to 1ms time base
 *                  by HAL_Init(), and its priority is set according to
 *                  NX_CONFIG_STM32_SYSTICK_PRIORITY.
 */
/* Platform hooks are private: only serialized nx_hal_init/deinit may invoke
 * them. The common HAL owns shutdown admission and records partial ownership. */
nx_status_t nx_platform_init(void) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (platform_ready) return NX_OK;
    if (platform_owned) return NX_ERR_INVALID_STATE;
    /* Mark ownership before the first vendor call, including failed HAL_Init. */
    platform_owned = true;
    if (HAL_Init() != HAL_OK) return NX_ERR_IO;
#ifdef NX_CONFIG_OSAL_BAREMETAL
    if (osal_baremetal_set_clock(HAL_GetTick) != OSAL_OK) return NX_ERR_BUSY;
    clock_bound = true;
#endif
    if ((uint32_t)*(volatile const uint16_t*)FLASHSIZE_BASE * 1024U !=
        NX_CONFIG_STM32_FLASH_SIZE) return NX_ERR_HARDWARE;
    if (SystemClock_Config() != 0) return NX_ERR_IO;
    if (nx_board_prepare_safe_outputs() != NX_OK) return NX_ERR_IO;
    HAL_NVIC_SetPriorityGrouping(7U - NX_CONFIG_STM32_NVIC_PRIORITY_GROUP);
    HAL_NVIC_SetPriority(SysTick_IRQn, NX_CONFIG_STM32_SYSTICK_PRIORITY, 0);
    HAL_NVIC_SetPriority(PendSV_IRQn, NX_CONFIG_STM32_PENDSV_PRIORITY, 0);
    /* DWT is optional diagnostics; its absence is not infrastructure failure. */
    (void)stm32_perf_init();
    stm32_boot_time_mark(3);
    platform_ready = true;
    return NX_OK;
}

nx_status_t nx_platform_init_check(void) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (osal_is_initialized()) return NX_ERR_BUSY;
#ifdef NX_CONFIG_OSAL_FREERTOS
    osal_execution_info_t execution;
    if (osal_get_execution_info(&execution) != OSAL_OK) return NX_ERR_IO;
    if (execution.scheduler_state != OSAL_SCHEDULER_NOT_STARTED) return NX_ERR_BUSY;
#endif
    return nx_stm32f407_resources_idle();
}

nx_status_t nx_platform_shutdown_check(void) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (!nx_device_shutdown_is_active()) return NX_ERR_INVALID_STATE;
    if (osal_is_initialized()) return NX_ERR_BUSY;
#ifdef NX_CONFIG_OSAL_FREERTOS
    osal_execution_info_t execution;
    if (osal_get_execution_info(&execution) != OSAL_OK) return NX_ERR_IO;
    if (execution.scheduler_state != OSAL_SCHEDULER_NOT_STARTED) return NX_ERR_BUSY;
#endif
    nx_status_t status = nx_device_provider_quiescence_check();
    if (status != NX_OK) return status;
    return nx_stm32f407_resources_idle();
}

nx_status_t nx_platform_deinit(void) {
    if (!platform_owned) return NX_OK;
    /* Read-only admission has already completed under the common HAL fence. */
    if (!nx_device_shutdown_is_active()) return NX_ERR_INVALID_STATE;
    platform_ready = false;
#ifdef NX_CONFIG_OSAL_BAREMETAL
    if (clock_bound) {
        if (osal_baremetal_clear_clock(HAL_GetTick) != OSAL_OK) return NX_ERR_BUSY;
        clock_bound = false;
    }
#endif
    nx_status_t status = nx_stm32f407_clock_release();
    if (status != NX_OK) return status;
    /* Neither fixed ST HAL_DeInit nor its weak RCC_DeInit stops SysTick. All
     * clock waits above are bounded register polls, independent of this tick. */
    SysTick->CTRL = 0u;
    SysTick->LOAD = 0u;
    SysTick->VAL = 0u;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    __DSB();
    __ISB();
    stm32_perf_deinit();
    HAL_StatusTypeDef vendor_status = HAL_DeInit();
    nx_status_t board_status = nx_board_prepare_safe_outputs();
    if (vendor_status != HAL_OK) return NX_ERR_IO;
    if (board_status != NX_OK) return board_status;
    platform_owned = false;
    return NX_OK;
}

int stm32_platform_init(void) {
    /* Legacy internal status wrapper; ordinary consumers use nx_hal_init. */
    return nx_platform_init() == NX_OK ? 0 : -1;
}

int stm32_platform_deinit(void) {
    /* Direct entry is rejected without the HAL's shutdown admission fence. */
    return nx_platform_deinit() == NX_OK ? 0 : -1;
}

/**
 * \brief           Get platform initialization status
 */
int stm32_platform_is_initialized(void) {
    return platform_ready ? 1 : 0;
}

/**
 * \brief           Get system core clock frequency
 */
uint32_t stm32_platform_get_sysclk(void) {
    return SystemCoreClock;
}

/**
 * \brief           HAL MSP initialization callback
 * \details         This function is called by HAL_Init() to perform low-level
 *                  initialization. It is implemented as a weak function to
 *                  allow user override for custom initialization.
 * \note            User can override this function in application code to
 *                  add custom initialization (e.g., enable peripheral clocks,
 *                  configure GPIO, etc.)
 */
__weak void HAL_MspInit(void) {
    /* User can add custom initialization code here */
    /* Example: Enable peripheral clocks, configure GPIO, etc. */
}
