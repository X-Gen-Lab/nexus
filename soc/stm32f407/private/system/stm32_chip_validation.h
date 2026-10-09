/**
 * \file            stm32_chip_validation.h
 * \brief           STM32 chip configuration validation interface
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

#ifndef NEXUS_STM32_CHIP_VALIDATION_H
#define NEXUS_STM32_CHIP_VALIDATION_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           STM32 chip feature enumeration
 */
typedef enum {
    STM32_FEATURE_FPU,       /**< Floating Point Unit */
    STM32_FEATURE_MPU,       /**< Memory Protection Unit */
    STM32_FEATURE_DCACHE,    /**< Data Cache */
    STM32_FEATURE_ICACHE,    /**< Instruction Cache */
    STM32_FEATURE_TRUSTZONE, /**< ARM TrustZone */
} stm32_chip_feature_t;

/**
 * \brief           Validate chip configuration at runtime
 * \return          0 on success, -1 on validation failure
 */
int stm32_chip_validate_config(void);

/**
 * \brief           Get chip series name
 * \return          String containing chip series name
 */
const char* stm32_chip_get_series_name(void);

/**
 * \brief           Get chip variant name
 * \return          String containing chip variant name
 */
const char* stm32_chip_get_variant_name(void);

/**
 * \brief           Check if chip has specific feature
 * \param[in]       feature: Feature to check
 * \return          true if feature is available, false otherwise
 */
bool stm32_chip_has_feature(stm32_chip_feature_t feature);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_STM32_CHIP_VALIDATION_H */
