/**
 * \file            stm32_chip_validation.c
 * \brief           STM32 chip configuration validation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-05
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Validates chip configuration at compile-time and runtime
 *                  - Chip model validity verification
 *                  - Memory configuration validation
 *                  - Feature flag consistency checks
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

#include "system/stm32_chip_validation.h"
#include <stdbool.h>
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* Compile-time validation                                                   */
/*---------------------------------------------------------------------------*/

/* Verify that a chip series is selected */
#if !defined(STM32C0) && !defined(STM32F0) && !defined(STM32F1) &&             \
    !defined(STM32F2) && !defined(STM32F3) && !defined(STM32F4) &&             \
    !defined(STM32F7) && !defined(STM32G0) && !defined(STM32G4) &&             \
    !defined(STM32H5) && !defined(STM32H7) && !defined(STM32L0) &&             \
    !defined(STM32L1) && !defined(STM32L4) && !defined(STM32L5) &&             \
    !defined(STM32U0) && !defined(STM32U3) && !defined(STM32U5) &&             \
    !defined(STM32WB) && !defined(STM32WL)
#error "No STM32 series selected. Please configure a series in Kconfig."
#endif

/* Verify that a specific chip variant is defined */
#if !defined(STM32F401xB) && !defined(STM32F401xC) && !defined(STM32F401xD) && \
    !defined(STM32F401xE) && !defined(STM32F405xx) && !defined(STM32F407xx) && \
    !defined(STM32F410Tx) && !defined(STM32F410Cx) && !defined(STM32F410Rx) && \
    !defined(STM32F411xC) && !defined(STM32F411xE) && !defined(STM32F412Cx) && \
    !defined(STM32F412Rx) && !defined(STM32F412Vx) && !defined(STM32F412Zx) && \
    !defined(STM32F413xx) && !defined(STM32F423xx) && !defined(STM32F427xx) && \
    !defined(STM32F437xx) && !defined(STM32F429xx) && !defined(STM32F439xx) && \
    !defined(STM32F446xx) && !defined(STM32F469xx) && !defined(STM32F479xx)
/* Add more chip variants as needed for other series */
#warning                                                                       \
    "Chip variant not explicitly defined. Build may succeed but features may be limited."
#endif

/* Verify memory configuration is defined */
#ifndef NX_CONFIG_STM32_FLASH_SIZE
#define NX_CONFIG_STM32_FLASH_SIZE 0x80000 /* Default 512KB */
#warning "Flash size not configured. Using default 512KB."
#endif

#ifndef NX_CONFIG_STM32_SRAM_SIZE
#define NX_CONFIG_STM32_SRAM_SIZE 0x20000 /* Default 128KB */
#warning "SRAM size not configured. Using default 128KB."
#endif

/*---------------------------------------------------------------------------*/
/* Runtime validation                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate chip configuration at runtime
 * \details         Performs runtime checks on chip configuration
 * \return          0 on success, -1 on validation failure
 */
int stm32_chip_validate_config(void) {
    /* Validate flash size is reasonable */
    if (NX_CONFIG_STM32_FLASH_SIZE == 0 ||
        NX_CONFIG_STM32_FLASH_SIZE > 0x200000) {
        return -1; /* Flash size out of reasonable range (0 - 2MB) */
    }

    /* Validate SRAM size is reasonable */
    if (NX_CONFIG_STM32_SRAM_SIZE == 0 || NX_CONFIG_STM32_SRAM_SIZE > 0x80000) {
        return -1; /* SRAM size out of reasonable range (0 - 512KB) */
    }

    /* Validate system clock frequency */
#ifdef NX_CONFIG_STM32_SYSCLK_FREQ
    if (NX_CONFIG_STM32_SYSCLK_FREQ == 0 ||
        NX_CONFIG_STM32_SYSCLK_FREQ > 480000000U) {
        return -1; /* System clock out of range (0 - 480MHz) */
    }
#endif

    /* Validate HSE value if enabled */
#ifdef NX_CONFIG_STM32_HSE_ENABLE
#ifdef NX_CONFIG_STM32_HSE_VALUE
    if (NX_CONFIG_STM32_HSE_VALUE < 4000000U ||
        NX_CONFIG_STM32_HSE_VALUE > 26000000U) {
        return -1; /* HSE frequency out of typical range (4-26MHz) */
    }
#endif
#endif

    return 0;
}

/**
 * \brief           Get chip series name
 * \return          String containing chip series name
 */
const char* stm32_chip_get_series_name(void) {
#if defined(STM32C0)
    return "STM32C0";
#elif defined(STM32F0)
    return "STM32F0";
#elif defined(STM32F1)
    return "STM32F1";
#elif defined(STM32F2)
    return "STM32F2";
#elif defined(STM32F3)
    return "STM32F3";
#elif defined(STM32F4)
    return "STM32F4";
#elif defined(STM32F7)
    return "STM32F7";
#elif defined(STM32G0)
    return "STM32G0";
#elif defined(STM32G4)
    return "STM32G4";
#elif defined(STM32H5)
    return "STM32H5";
#elif defined(STM32H7)
    return "STM32H7";
#elif defined(STM32L0)
    return "STM32L0";
#elif defined(STM32L1)
    return "STM32L1";
#elif defined(STM32L4)
    return "STM32L4";
#elif defined(STM32L5)
    return "STM32L5";
#elif defined(STM32U0)
    return "STM32U0";
#elif defined(STM32U3)
    return "STM32U3";
#elif defined(STM32U5)
    return "STM32U5";
#elif defined(STM32WB)
    return "STM32WB";
#elif defined(STM32WL)
    return "STM32WL";
#else
    return "Unknown";
#endif
}

/**
 * \brief           Get chip variant name
 * \return          String containing chip variant name
 */
const char* stm32_chip_get_variant_name(void) {
#ifdef NX_CONFIG_STM32_CHIP_NAME
    return NX_CONFIG_STM32_CHIP_NAME;
#else
    return "Unknown";
#endif
}

/**
 * \brief           Check if chip has specific feature
 * \param[in]       feature: Feature to check
 * \return          true if feature is available, false otherwise
 */
bool stm32_chip_has_feature(stm32_chip_feature_t feature) {
    switch (feature) {
        case STM32_FEATURE_FPU:
#ifdef NX_CONFIG_STM32_HAS_FPU
            return true;
#else
            return false;
#endif

        case STM32_FEATURE_MPU:
#ifdef NX_CONFIG_STM32_HAS_MPU
            return true;
#else
            return false;
#endif

        case STM32_FEATURE_DCACHE:
#ifdef NX_CONFIG_STM32_HAS_DCACHE
            return true;
#else
            return false;
#endif

        case STM32_FEATURE_ICACHE:
#ifdef NX_CONFIG_STM32_HAS_ICACHE
            return true;
#else
            return false;
#endif

        case STM32_FEATURE_TRUSTZONE:
#ifdef NX_CONFIG_STM32_HAS_TRUSTZONE
            return true;
#else
            return false;
#endif

        default:
            return false;
    }
}
