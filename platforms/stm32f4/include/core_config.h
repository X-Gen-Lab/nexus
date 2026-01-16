/**
 * \file            core_config.h
 * \brief           Cortex-M Core Configuration and Feature Detection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \note            This header provides core type detection and feature
 *                  detection macros for Cortex-M series processors.
 *                  Supports CM0, CM0+, CM3, CM4, CM7, and CM33 cores.
 *
 * \par             Requirements: 13.1, 13.2, 13.9
 */

#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Core Type Definitions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Cortex-M core type identifiers
 */
#define CORE_CM0  0  /**< Cortex-M0 core */
#define CORE_CM0P 1  /**< Cortex-M0+ core */
#define CORE_CM3  3  /**< Cortex-M3 core */
#define CORE_CM4  4  /**< Cortex-M4 core */
#define CORE_CM7  7  /**< Cortex-M7 core */
#define CORE_CM33 33 /**< Cortex-M33 core */

/*---------------------------------------------------------------------------*/
/* Core Type Detection                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Detect current Cortex-M core type
 * \note            Detection is based on CMSIS __CORTEX_M macro defined
 *                  by device headers or compiler predefined macros.
 */
#ifndef CORE_TYPE
#if defined(__CORTEX_M)
#if (__CORTEX_M == 4)
#define CORE_TYPE CORE_CM4
#elif (__CORTEX_M == 7)
#define CORE_TYPE CORE_CM7
#elif (__CORTEX_M == 33)
#define CORE_TYPE CORE_CM33
#elif (__CORTEX_M == 3)
#define CORE_TYPE CORE_CM3
#elif (__CORTEX_M == 0)
#if defined(__CM0PLUS_REV)
#define CORE_TYPE CORE_CM0P
#else
#define CORE_TYPE CORE_CM0
#endif
#else
#error "Unknown Cortex-M core type from __CORTEX_M"
#endif
#elif defined(__ARM_ARCH_7EM__)
/* Cortex-M4 or M7 with DSP extension */
#if defined(__ARM_ARCH_7M__)
#define CORE_TYPE CORE_CM3
#else
#define CORE_TYPE CORE_CM4
#endif
#elif defined(__ARM_ARCH_7M__)
#define CORE_TYPE CORE_CM3
#elif defined(__ARM_ARCH_6M__)
#define CORE_TYPE CORE_CM0
#elif defined(__ARM_ARCH_8M_MAIN__)
#define CORE_TYPE CORE_CM33
#elif defined(__ARM_ARCH_8M_BASE__)
#define CORE_TYPE CORE_CM0P
#elif defined(STM32F4)
/* Fallback for STM32F4 series - always Cortex-M4 */
#define CORE_TYPE CORE_CM4
#else
/* Default to CM4 for STM32F4 HAL adapter */
#define CORE_TYPE CORE_CM4
#endif
#endif /* CORE_TYPE */

/*---------------------------------------------------------------------------*/
/* Feature Detection Macros                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           FPU (Floating Point Unit) support detection
 * \note            FPU is available on CM4, CM7, and CM33 cores
 */
#if (CORE_TYPE == CORE_CM4) || (CORE_TYPE == CORE_CM7) ||                      \
    (CORE_TYPE == CORE_CM33)
#define CORE_HAS_FPU 1
#else
#define CORE_HAS_FPU 0
#endif

/**
 * \brief           DSP (Digital Signal Processing) instructions support
 * \note            DSP instructions are available on CM4, CM7, and CM33 cores
 */
#if (CORE_TYPE == CORE_CM4) || (CORE_TYPE == CORE_CM7) ||                      \
    (CORE_TYPE == CORE_CM33)
#define CORE_HAS_DSP 1
#else
#define CORE_HAS_DSP 0
#endif

/**
 * \brief           MPU (Memory Protection Unit) support detection
 * \note            MPU is available on CM3 and above cores
 */
#if (CORE_TYPE >= CORE_CM3)
#define CORE_HAS_MPU 1
#else
#define CORE_HAS_MPU 0
#endif

/**
 * \brief           Cache support detection
 * \note            Cache is only available on CM7 cores
 */
#if (CORE_TYPE == CORE_CM7)
#define CORE_HAS_CACHE 1
#else
#define CORE_HAS_CACHE 0
#endif

/**
 * \brief           TrustZone support detection
 * \note            TrustZone is only available on CM33 cores
 */
#if (CORE_TYPE == CORE_CM33)
#define CORE_HAS_TZ 1
#else
#define CORE_HAS_TZ 0
#endif

/*---------------------------------------------------------------------------*/
/* NVIC Priority Configuration                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           NVIC priority bits based on core type
 * \note            CM0/CM0+ have 2 bits, CM3/CM4/CM7/CM33 have 4 bits
 *                  (implementation defined, typically 4 for STM32)
 */
#if (CORE_TYPE == CORE_CM0) || (CORE_TYPE == CORE_CM0P)
#define CORE_NVIC_PRIO_BITS 2
#elif (CORE_TYPE == CORE_CM3) || (CORE_TYPE == CORE_CM4)
#define CORE_NVIC_PRIO_BITS 4
#elif (CORE_TYPE == CORE_CM7) || (CORE_TYPE == CORE_CM33)
#define CORE_NVIC_PRIO_BITS 4 /**< Configurable, default 4 */
#else
#define CORE_NVIC_PRIO_BITS 4 /**< Default fallback */
#endif

/**
 * \brief           Maximum NVIC priority value
 */
#define CORE_NVIC_PRIO_MAX ((1UL << CORE_NVIC_PRIO_BITS) - 1)

/**
 * \brief           Lowest NVIC priority (highest numerical value)
 */
#define CORE_NVIC_PRIO_LOWEST CORE_NVIC_PRIO_MAX

/**
 * \brief           Highest NVIC priority (lowest numerical value)
 */
#define CORE_NVIC_PRIO_HIGHEST 0

/*---------------------------------------------------------------------------*/
/* Core Feature String Macros (for debugging/logging)                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get core type name as string
 */
#if (CORE_TYPE == CORE_CM0)
#define CORE_TYPE_STRING "Cortex-M0"
#elif (CORE_TYPE == CORE_CM0P)
#define CORE_TYPE_STRING "Cortex-M0+"
#elif (CORE_TYPE == CORE_CM3)
#define CORE_TYPE_STRING "Cortex-M3"
#elif (CORE_TYPE == CORE_CM4)
#define CORE_TYPE_STRING "Cortex-M4"
#elif (CORE_TYPE == CORE_CM7)
#define CORE_TYPE_STRING "Cortex-M7"
#elif (CORE_TYPE == CORE_CM33)
#define CORE_TYPE_STRING "Cortex-M33"
#else
#define CORE_TYPE_STRING "Unknown"
#endif

/*---------------------------------------------------------------------------*/
/* Compile-time Assertions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Static assertion helper for core configuration validation
 */
#ifndef CORE_STATIC_ASSERT
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define CORE_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
#define CORE_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define CORE_STATIC_ASSERT(cond, msg)                                          \
    typedef char core_static_assert_##__LINE__[(cond) ? 1 : -1]
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* CORE_CONFIG_H */
