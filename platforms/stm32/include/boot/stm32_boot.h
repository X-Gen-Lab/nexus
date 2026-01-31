/**
 * \file            stm32_boot.h
 * \brief           STM32 platform boot and initialization interface
 * \author          Nexus Team
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

#ifndef NEXUS_STM32_BOOT_H
#define NEXUS_STM32_BOOT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Boot configuration parameters
 */
typedef struct {
    uint32_t stack_size;  /**< Stack size (bytes) */
    uint32_t heap_size;   /**< Heap size (bytes) */
    bool enable_fpu;      /**< Enable FPU */
    bool enable_icache;   /**< Enable instruction cache */
    bool enable_dcache;   /**< Enable data cache */
    uint32_t vtor_offset; /**< Vector table offset */
} boot_config_t;

/**
 * \brief           Initialize STM32 platform
 * \return          0 on success, -1 on failure
 */
int stm32_platform_init(void);

/**
 * \brief           Deinitialize STM32 platform
 * \return          0 on success, -1 on failure
 */
int stm32_platform_deinit(void);

/**
 * \brief           Get platform initialization status
 * \return          1 if initialized, 0 otherwise
 */
int stm32_platform_is_initialized(void);

/**
 * \brief           Get system core clock frequency
 * \return          System clock frequency in Hz
 */
uint32_t stm32_platform_get_sysclk(void);

/**
 * \brief           HAL MSP initialization callback
 */
void HAL_MspInit(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_STM32_BOOT_H */
